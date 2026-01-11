#include "sstable.h"
#include <iostream>
#include <fstream>
#include <algorithm>

std::vector<IndexEntry> SSTable::flush(const std::map<std::string, std::string>& data, const std::string& filename, BloomFilter& bf) {
    std::ofstream file(filename, std::ios::binary);
    std::vector<IndexEntry> sparse_index;

    if (!file.is_open()) {
        std::cerr << "Failed to open SSTable file: " << filename << std::endl;
        return sparse_index;
    }

    long current_offset = 0;
    int counter = 0;
    int BLOCK_SIZE = 100;

    for (const auto& [key, value] : data) {
        int key_len = key.size();
        int value_len = value.size();

        if (counter % BLOCK_SIZE == 0) {
            sparse_index.push_back({key, current_offset});
        }

        file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        file.write(key.c_str(), key_len);

        file.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
        file.write(value.c_str(), value_len);

        bf.add(key);

        current_offset += (sizeof(int) + key.size() + sizeof(int) + value.size());
        counter++;
    }

    file.close();
    std::cout << "Flushed SSTable to " << filename << " (" << data.size() << " keys)" << std::endl;

    return sparse_index;
};

std::vector<IndexEntry> SSTable::loadIndex(const std::string& filename, BloomFilter& bf) {
    std::ifstream file(filename, std::ios::binary);
    std::vector<IndexEntry> sparse_index;

    if (!file.is_open()) {
        std::cerr << "Failed to open SSTable file: " << filename << std::endl;
        return sparse_index;
    }

    long current_offset = 0;
    int counter = 0;
    int BLOCK_SIZE = 100;

    while (file.peek() != EOF) {
        long entry_offset = current_offset;

        int key_len = 0;
        file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));

        if (file.eof()) break;

        std::string key(key_len, '\0');
        file.read(&key[0], key_len);

        int value_len = 0;
        file.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));

        file.seekg(value_len, std::ios::cur);

        bf.add(key);

        if (counter % BLOCK_SIZE == 0) {
            sparse_index.push_back({key, entry_offset});
        }

        current_offset = file.tellg();
        counter++;
    }

    file.close();
    return sparse_index;
}

bool SSTable::search(const std::string& filename, const std::vector<IndexEntry>& index, const std::string& key, std::string& value) {
    auto entry = std::upper_bound(index.begin(), index.end(), key, [](const std::string& k, const IndexEntry& e) {
        return k < e.key;
    });

    if (entry == index.begin()) {
        return false;
    }

    auto block_start = std::prev(entry);
    long start_offset = block_start->offset;

    std::cout << "[DEBUG] Search(" << key << ") -> Index Jumped to Offset: " << start_offset << std::endl;

    long end_offset = -1;
    if (entry != index.end()) {
        end_offset = entry->offset;
    }
    
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Failed to open SSTable file: " << filename << std::endl;
        return false;
    }

    file.seekg(start_offset);

    while (file.peek() != EOF) {
        if (end_offset != -1 && file.tellg() >= end_offset) {
            break; 
        }

        int key_len = 0;
        int value_len = 0;

        file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));

        if (file.eof()) break;

        std::string current_key(key_len, '\0');
        file.read(&current_key[0], key_len);

        file.read(reinterpret_cast<char*>(&value_len), sizeof(value_len));

        std::string current_value(value_len, '\0');
        file.read(&current_value[0], value_len);

        if (current_key == key) {
            value = current_value;
            return true;
        }
    }

    return false;
}