#include "kvstore.h"
#include "sstable.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

KVStore::KVStore(const std::string& filename, const std::string& directory) {
    wal = std::make_unique<WAL>(filename);
    memtable = std::make_unique<MemTable>();

    std::vector<std::pair<std::string, std::string>> history = wal->readAll();

    for (const auto& [key, value] : history) {
        memtable->put(key, value);
    }

    if (!history.empty()) {
        std::cout << "Loaded " << history.size() << " entries from WAL" << std::endl;
    } else {
        std::cout << "No history found in WAL" << std::endl;
    }

    if (!fs::exists(directory)) {
        fs::create_directory(directory);
    }

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (entry.path().extension() == ".sst") {
            sstables.push_back(entry.path().string());
        }
    }

    std::sort(sstables.begin(), sstables.end());

    std::cout << "Startup: Found " << sstables.size() << " existing SSTables in " << directory << std::endl;
}

void KVStore::put(const std::string& key, const std::string& value) {
    bool success = wal->write(key, value);

    if (!success) {
        std::cerr << "Failed to write to WAL" << std::endl;
        return;
    }

    memtable->put(key, value);

    if (memtable->size() >= 3) {
        std::cout << "MemTable full! Flushing to disk..." << std::endl;

        std::map<std::string, std::string> data = memtable->flush();

        std::string new_filename = "data_" + std::to_string(sstables.size() + 1) + ".sst";

        SSTable::flush(data, new_filename);
        
        sstables.push_back(new_filename);

        wal->clear();
    }
}

std::optional<std::string> KVStore::get(const std::string& key) const {
    auto result = memtable->get(key);

    if (result) {
        if (*result == "TOMBSTONE") {
            return std::nullopt;
        }

        return result;
    }

    for (auto it = sstables.rbegin(); it != sstables.rend(); ++it) {
        std::string value;

        if (SSTable::search(*it, key, value)) {
            if (value == "TOMBSTONE") {
                return std::nullopt;
            }

            return value;
        }
    }

    return std::nullopt;
};

void KVStore::remove(const std::string& key) {
    wal->write(key, "TOMBSTONE");
    memtable->put(key, "TOMBSTONE");
};