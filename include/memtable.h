#pragma once
#include <string>
#include <map>
#include <shared_mutex>
#include <optional>

class MemTable {
    public:
        MemTable();

        ~MemTable();

        void put(const std::string& key, const std::string& value);
        
        std::optional<std::string> get(const std::string& key) const;

        void remove(const std::string& key);

        size_t size() const;

        void clear();

        std::map<std::string, std::string> flush();

    private:
        std::map<std::string, std::string> table;
        mutable std::shared_mutex rw_mutex;
};