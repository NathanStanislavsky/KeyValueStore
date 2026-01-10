#include "sstable.h"
#include <iostream>
#include <fstream>

std::vector<IndexEntry> SSTable::flush(const std::map<std::string, std::string>& data, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    std::vector<IndexEntry> sparse_index;

    if (!file.is_open()) {
        std::cerr << "Failed to open SSTable file: " << filename << std::endl;
        return;
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

        current_offset += (sizeof(int) + key.size() + sizeof(int) + value.size());
        counter++;
    }

    file.close();
    std::cout << "Flushed SSTable to " << filename << " (" << data.size() << " keys)" << std::endl;

    return sparse_index;
};

bool SSTable::search(const std::string& filename, const std::string& key, std::string& value) {
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Failed to open SSTable file: " << filename << std::endl;
        return false;
    }

    while (file.peek() != EOF) {
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