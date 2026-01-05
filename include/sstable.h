#pragma once
#include <string>
#include <map>
#include <vector>

class SSTable {
    public:
        static void flush(const std::map<std::string, std::string>& data, const std::string& filename);
};