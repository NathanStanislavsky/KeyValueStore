#pragma once
#include <string>
#include <optional>
#include <memory>
#include <shared_mutex>
#include <set>
#include "memtable.h"
#include "wal.h"
#include "sstable.h"
#include "bloomfilter.h"

struct SSTableMetadata
{
    std::string filename;
    std::vector<IndexEntry> index;
    BloomFilter bloomFilter;

    int fileId;
    std::string minKey;
    std::string maxKey;
    long fileSize;

    bool operator<(const SSTableMetadata &other) const
    {
        return filename < other.filename;
    }
};

class KVStore
{
public:
    KVStore(const std::string &filename, const std::string &directory);

    void put(const std::string &key, const std::string &value);

    std::optional<std::string> get(const std::string &key) const;

    void remove(const std::string &key);

private:
    std::unique_ptr<MemTable> memtable;
    std::unique_ptr<WAL> wal;
    std::vector<std::vector<SSTableMetadata>> levels;
    std::string data_directory;
    mutable std::shared_mutex levels_mutex;
    std::set<int> active_compactions;

    void checkCompactionStatus();
    void compact(int level);
    void loadSSTables();
    std::string generateSSTableFilename(int level, int file_id);
};