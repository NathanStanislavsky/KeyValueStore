#pragma once
#include <string>
#include <optional>
#include <memory>
#include "memtable.h"
#include "wal.h"

class KVStore {
    public:
        KVStore(const std::string& directory);

        void put(const std::string& key, std::string& value);
        
        std::optional<std::string> get(const std::string& key) const;

        void remove(const std::string& key);

    private:
        std::unique_ptr<MemTable> memtable;
        std::unique_ptr<WAL> wal;
};