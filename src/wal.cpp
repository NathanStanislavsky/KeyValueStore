#include "wal.h"
#include <iostream>

WAL::WAL(const std::string &filename)
{
    file_stream.open(filename, std::ios::out | std::ios::app | std::ios::binary);

    if (!file_stream.is_open())
    {
        std::cerr << "Failed to open WAL file: " << filename << std::endl;
    }
}

WAL::~WAL() {
    if (file_stream.is_open()) {
        file_stream.close();
    }
}

bool WAL::write(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(log_mutex);

    int key_len = key.size();
    int value_len = value.size();

    file_stream.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
    file_stream.write(key.c_str(), key_len);

    file_stream.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
    file_stream.write(value.c_str(), value_len);

    file_stream.flush();

    return file_stream.good();
}