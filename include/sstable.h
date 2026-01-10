#pragma once
#include <string>
#include <map>
#include <vector>

struct IndexEntry {
    std::string key;
    long offset;
};

class SSTable {
    public:
        static std::vector<IndexEntry> flush(const std::map<std::string, std::string>& data, const std::string& filename);

        static bool search(const std::string& filename, const std::string& key, std::string& value);
};