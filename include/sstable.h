#pragma once
#include <string>
#include <map>
#include <vector>
#include "bloomfilter.h"

struct IndexEntry {
    std::string key;
    long offset;
};

class SSTable {
    public:
        static std::vector<IndexEntry> flush(const std::map<std::string, std::string>& data, const std::string& filename, BloomFilter& bf);

        static std::vector<IndexEntry> loadIndex(const std::string& filename, BloomFilter& bf);

        static bool search(const std::string& filename, const std::vector<IndexEntry>& index, const std::string& key, std::string& value);
};