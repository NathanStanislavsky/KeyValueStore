#include "memtable.h"
#include <iostream>

MemTable::MemTable() {}

MemTable::~MemTable() {
    table.clear();
}

void MemTable::put(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);
    table[key] = value;
}

std::optional<std::string> MemTable::get(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);

    auto iterator = table.find(key);

    if (iterator != table.end()) {
        return iterator->second;
    }

    return std::nullopt;
};

void MemTable::remove(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);

    table.erase(key);
}

size_t MemTable::size() const {
    std::shared_lock<std::shared_mutex> lock(rw_mutex);

    return table.size();
}

void MemTable::clear() {
    std::unique_lock<std::shared_mutex> lock(rw_mutex);

    table.clear();
}