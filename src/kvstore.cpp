#include "kvstore.h"
#include "sstable.h"
#include "SSTableIterator.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <queue>
#include <sstream>
#include <set>
#include <cstdio>

namespace fs = std::filesystem;

KVStore::KVStore(const std::string &filename, const std::string &directory) : data_directory(directory)
{
    wal = std::make_unique<WAL>(filename);
    memtable = std::make_unique<MemTable>();

    std::vector<std::pair<std::string, std::string>> history = wal->readAll();

    for (const auto &[key, value] : history)
    {
        memtable->put(key, value);
    }

    if (!history.empty())
    {
        std::cout << "Loaded " << history.size() << " entries from WAL" << std::endl;
    }
    else
    {
        std::cout << "No history found in WAL" << std::endl;
    }

    if (!fs::exists(directory))
    {
        fs::create_directory(directory);
    }

    loadSSTables();
}

void KVStore::loadSSTables()
{
    levels.clear();
    levels.push_back({});

    int max_level = 0;
    std::vector<std::pair<int, SSTableMetadata>> candidates;

    for (const auto &entry : fs::directory_iterator(data_directory))
    {
        if (entry.path().extension() == ".sst")
        {
            if (!fs::exists(entry.path()))
            {
                continue;
            }

            std::string filename = entry.path().filename().string();

            int level = 0;
            int fileId = 0;

            if (filename.find("level_") == 0)
            {
                if (sscanf(filename.c_str(), "level_%d_%d.sst", &level, &fileId) != 2)
                {
                    continue;
                }
            }

            std::string full_path = fs::absolute(entry.path()).lexically_normal().string();
            BloomFilter bf(1000, 7);
            std::vector<IndexEntry> index = SSTable::loadIndex(full_path, bf);

            std::string min_key = "";
            std::string max_key = "";
            if (!index.empty())
            {
                min_key = index.front().key;
                max_key = index.back().key;
            }
            long file_size = fs::file_size(entry.path());

            SSTableMetadata metadata = {full_path, index, bf, fileId, min_key, max_key, file_size};

            candidates.push_back({level, metadata});
            max_level = std::max(max_level, level);
        }
    }

    while (levels.size() <= max_level)
    {
        levels.push_back({});
    }

    for (auto &[level, metadata] : candidates)
    {
        levels[level].push_back(metadata);
    }

    for (auto &level : levels)
    {
        std::sort(level.begin(), level.end(),
                  [](const SSTableMetadata &a, const SSTableMetadata &b)
                  {
                      return a.fileId < b.fileId;
                  });
    }

    std::cout << "Loaded " << candidates.size() << " SSTables across " << (max_level + 1) << " levels" << std::endl;
}

std::string KVStore::generateSSTableFilename(int level, int file_id)
{
    std::ostringstream oss;
    oss << data_directory << "/level_" << level << "_" << file_id << ".sst";
    std::string path = oss.str();
    return fs::absolute(path).lexically_normal().string();
}

void KVStore::put(const std::string &key, const std::string &value)
{
    bool success = wal->write(key, value);

    if (!success)
    {
        std::cerr << "Failed to write to WAL" << std::endl;
        return;
    }

    memtable->put(key, value);

    if (memtable->size() >= 1000)
    {
        wal->rotate();

        std::map<std::string, std::string> data = memtable->flush();

        std::string new_filename;
        int newFileId;

        {
            std::unique_lock<std::shared_mutex> lock(levels_mutex);

            if (levels.empty())
            {
                levels.push_back({});
            }

            newFileId = levels[0].empty() ? 1 : (std::max_element(levels[0].begin(), levels[0].end(), [](const SSTableMetadata &a, const SSTableMetadata &b)
                                                                  { return a.fileId < b.fileId; })
                                                     ->fileId +
                                                 1);

            new_filename = generateSSTableFilename(0, newFileId);
        }

        if (data.empty()) {
            return;
        }
        BloomFilter bf(data.size(), 7);
        std::vector<IndexEntry> index = SSTable::flush(data, new_filename, bf);
        long file_size = fs::file_size(fs::path(new_filename));
        SSTableMetadata metadata = {new_filename, index, bf, newFileId, data.begin()->first, data.rbegin()->first, file_size};

        {
            std::unique_lock<std::shared_mutex> lock(levels_mutex);
            levels[0].push_back(metadata);
        }

        wal->clearTemp();

        checkCompactionStatus();
    }
}

std::optional<std::string> KVStore::get(const std::string &key) const
{
    auto result = memtable->get(key);
    if (result)
    {
        return *result == "TOMBSTONE" ? std::nullopt : result;
    }

    std::shared_lock<std::shared_mutex> lock(levels_mutex);

    if (!levels.empty() && !levels[0].empty())
    {
        for (auto it = levels[0].rbegin(); it != levels[0].rend(); ++it)
        {
            if (!it->minKey.empty() && !it->maxKey.empty() && (key < it->minKey || key > it->maxKey))
            {
                continue;
            }

            if (!it->bloomFilter.contains(key))
            {
                continue;
            }

            std::string value;
            if (SSTable::search(it->filename, it->index, key, value))
            {
                return value == "TOMBSTONE" ? std::nullopt : std::make_optional(value);
            }
        }
    }

    for (size_t i = 1; i < levels.size(); ++i)
    {
        const auto &level_files = levels[i];
        if (level_files.empty())
        {
            continue;
        }

        auto it = std::lower_bound(level_files.begin(), level_files.end(), key,
                                   [](const SSTableMetadata &meta, const std::string &val)
                                   {
                                       return meta.maxKey < val;
                                   });

        if (it != level_files.end() && key >= it->minKey && key <= it->maxKey)
        {
            if (it->bloomFilter.contains(key))
            {
                std::string value;
                if (SSTable::search(it->filename, it->index, key, value))
                {
                    return value == "TOMBSTONE" ? std::nullopt : std::make_optional(value);
                }
            }
        }
    }

    return std::nullopt;
}

void KVStore::remove(const std::string &key)
{
    wal->write(key, "TOMBSTONE");
    memtable->put(key, "TOMBSTONE");
};

void KVStore::checkCompactionStatus()
{
    int levelToCompact = -1;
    size_t fileCount = 0;

    {
        std::shared_lock<std::shared_mutex> lock(levels_mutex);

        if (levels.size() > 0 && levels[0].size() > 4)
        {
            if (active_compactions.find(0) == active_compactions.end())
            {
                levelToCompact = 0;
                fileCount = levels[0].size();
            }
        }
        else
        {
            for (size_t level = 1; level < levels.size(); ++level)
            {
                if (levels[level].size() > 10)
                {
                    if (active_compactions.find(level) == active_compactions.end())
                    {
                        levelToCompact = level;
                        fileCount = levels[level].size();
                        break;
                    }
                }
            }
        }
    }

    if (levelToCompact >= 0)
    {
        compact(levelToCompact);
    }
}

void KVStore::compact(int level)
{
    {
        std::unique_lock<std::shared_mutex> lock(levels_mutex);
        if (active_compactions.find(level) != active_compactions.end())
        {
            return;
        }
        active_compactions.insert(level);
    }

    std::vector<SSTableMetadata> toCompact;
    std::vector<SSTableMetadata> nextLevelOverlapping;
    bool needNewLevel = false;
    int startFileId = 1;

    {
        std::shared_lock<std::shared_mutex> lock(levels_mutex);
        if (level >= levels.size() || levels[level].empty())
        {
            lock.unlock();
            std::unique_lock<std::shared_mutex> writeLock(levels_mutex);
            active_compactions.erase(level);
            return;
        }

        if (level + 1 >= levels.size())
        {
            needNewLevel = true;
        }
        else
        {
            if (!levels[level + 1].empty())
            {
                startFileId = std::max_element(levels[level + 1].begin(), levels[level + 1].end(),
                                               [](const SSTableMetadata &a, const SSTableMetadata &b)
                                               {
                                                   return a.fileId < b.fileId;
                                               })
                                  ->fileId +
                              1;
            }
        }

        toCompact = levels[level];

        if (level + 1 < levels.size() && !levels[level + 1].empty() && !toCompact.empty())
        {
            std::string minKey = toCompact[0].minKey;
            std::string maxKey = toCompact[0].maxKey;

            for (const auto &sst : toCompact)
            {
                if (sst.minKey < minKey)
                    minKey = sst.minKey;
                if (sst.maxKey > maxKey)
                    maxKey = sst.maxKey;
            }

            for (const auto &sst : levels[level + 1])
            {
                bool overlaps = !(sst.maxKey < minKey || sst.minKey > maxKey);
                if (overlaps)
                {
                    nextLevelOverlapping.push_back(sst);
                }
            }
        }
    }

    struct IteratorWrapper
    {
        std::unique_ptr<SSTableIterator> iter;
        int fileId;
        int level;

        bool operator>(const IteratorWrapper &other) const
        {
            if (iter->key() != other.iter->key())
            {
                return iter->key() > other.iter->key();
            }
            if (level != other.level)
            {
                return level > other.level;
            }

            return fileId < other.fileId;
        }
    };

    std::priority_queue<IteratorWrapper, std::vector<IteratorWrapper>,
                        std::greater<IteratorWrapper>>
        minHeap;

    for (const auto &sst : toCompact)
    {
        auto iter = std::make_unique<SSTableIterator>(sst.filename, sst.fileId);
        if (iter->hasNext())
        {
            minHeap.push({std::move(iter), sst.fileId, level});
        }
    }

    for (const auto &sst : nextLevelOverlapping)
    {
        auto iter = std::make_unique<SSTableIterator>(sst.filename, sst.fileId);
        if (iter->hasNext())
        {
            minHeap.push({std::move(iter), sst.fileId, level + 1});
        }
    }

    bool isBottomLevel = (level + 1 >= levels.size() - 1);

    const size_t MAX_SSTABLE_SIZE = 2 * 1024 * 1024;
    std::vector<std::pair<std::string, std::string>> currentBatch;
    size_t currentBatchSize = 0;

    int newFileId = startFileId;
    std::vector<SSTableMetadata> newSegmentFiles;

    std::string lastKey = "";
    bool isFirst = true;

    auto flushBatch = [&]()
    {
        if (currentBatch.empty())
            return;

        BloomFilter bf(currentBatch.size(), 7);
        std::string filename = generateSSTableFilename(level + 1, newFileId);
        std::vector<IndexEntry> index = SSTable::flush(currentBatch, filename, bf);

        SSTableMetadata metadata = {
            filename,
            index,
            bf,
            newFileId,
            currentBatch.front().first,
            currentBatch.back().first,
            static_cast<long>(fs::file_size(filename))};

        newSegmentFiles.push_back(metadata);
        newFileId++;
        currentBatch.clear();
        currentBatchSize = 0;
    };

    while (!minHeap.empty())
    {
        IteratorWrapper top = std::move(const_cast<IteratorWrapper &>(minHeap.top()));
        minHeap.pop();

        std::string key = top.iter->key();
        std::string value = top.iter->value();

        if (!isFirst && key == lastKey)
        {
            top.iter->next();
            if (top.iter->hasNext())
            {
                minHeap.push(std::move(top));
            }
            continue;
        }

        if (value == "TOMBSTONE" && isBottomLevel)
        {
            lastKey = key;
            isFirst = false;

            top.iter->next();
            if (top.iter->hasNext())
            {
                minHeap.push(std::move(top));
            }
            continue;
        }

        size_t entrySize = sizeof(int) + key.size() + sizeof(int) + value.size();

        if (!currentBatch.empty() && currentBatchSize + entrySize > MAX_SSTABLE_SIZE)
        {
            flushBatch();
        }

        currentBatch.push_back({key, value});
        currentBatchSize += entrySize;

        lastKey = key;
        isFirst = false;

        top.iter->next();
        if (top.iter->hasNext())
        {
            minHeap.push(std::move(top));
        }
    }

    flushBatch();

    {
        std::unique_lock<std::shared_mutex> lock(levels_mutex);

        if (needNewLevel)
        {
            levels.push_back({});
        }

        std::set<std::string> compactedFilenames;
        for (const auto &sst : toCompact)
        {
            compactedFilenames.insert(sst.filename);
        }

        levels[level].erase(
            std::remove_if(levels[level].begin(), levels[level].end(),
                           [&compactedFilenames](const SSTableMetadata &meta)
                           {
                               return compactedFilenames.find(meta.filename) != compactedFilenames.end();
                           }),
            levels[level].end());

        std::set<std::string> overlappingFilenames;
        for (const auto &sst : nextLevelOverlapping)
        {
            overlappingFilenames.insert(sst.filename);
        }

        levels[level + 1].erase(
            std::remove_if(levels[level + 1].begin(), levels[level + 1].end(),
                           [&overlappingFilenames](const SSTableMetadata &meta)
                           {
                               return overlappingFilenames.find(meta.filename) != overlappingFilenames.end();
                           }),
            levels[level + 1].end());

        levels[level + 1].insert(levels[level + 1].end(),
                                 newSegmentFiles.begin(), newSegmentFiles.end());

        std::sort(levels[level + 1].begin(), levels[level + 1].end(),
                  [](const SSTableMetadata &a, const SSTableMetadata &b)
                  {
                      return a.minKey < b.minKey;
                  });
    }

    for (const auto &sst : toCompact)
    {
        fs::remove(sst.filename);
    }
    for (const auto &sst : nextLevelOverlapping)
    {
        fs::remove(sst.filename);
    }

    {
        std::unique_lock<std::shared_mutex> lock(levels_mutex);
        active_compactions.erase(level);
    }

    checkCompactionStatus();
}