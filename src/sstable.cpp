#include "sstable.h"
#include <iostream>
#include <fstream>

void SSTable::flush(const std::map<std::string, std::string>& data, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Failed to open SSTable file: " << filename << std::endl;
        return;
    }

    for (const auto& [key, value] : data) {
        int key_len = key.size();
        int value_len = value.size();

        file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        file.write(key.c_str(), key_len);

        file.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
        file.write(value.c_str(), value_len);
    }

    file.close();
    std::cout << "Flushed SSTable to " << filename << " (" << data.size() << " keys)" << std::endl;
};