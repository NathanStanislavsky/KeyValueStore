#include "kvstore.h"
#include <iostream>

KVStore::KVStore(const std::string& filename) {
    wal = std::make_unique<WAL>(filename);
    memtable = std::make_unique<MemTable>();
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