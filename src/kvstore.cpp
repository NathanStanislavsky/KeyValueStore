#include "kvstore.h"
#include <iostream>

KVStore::KVStore(const std::string& filename) {
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
}

void KVStore::put(const std::string& key, const std::string& value) {
    bool success = wal->write(key, value);

    if (!success) {
        std::cerr << "Failed to write to WAL" << std::endl;
        return;
    }

    memtable->put(key, value);
}

std::optional<std::string> KVStore::get(const std::string& key) const {
    return memtable->get(key);
};

void KVStore::remove(const std::string& key) {
    wal->write(key, "DELETED");
    memtable->remove(key);
};