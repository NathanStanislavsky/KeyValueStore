#pragma once
#include <string>
#include <optional>
#include <memory>
#include "memtable.h"
#include "wal.h"
#include "sstable.h"

struct SSTableMetadata {
    std::string filename;
    std::vector<IndexEntry> index;
};

class KVStore {
    public:
        KVStore(const std::string& filename, const std::string& directory);

        void put(const std::string& key, const std::string& value);
        
        std::optional<std::string> get(const std::string& key) const;

        void remove(const std::string& key);

    private:
        std::unique_ptr<MemTable> memtable;
        std::unique_ptr<WAL> wal;
        std::vector<SSTableMetadata> sstables;
};