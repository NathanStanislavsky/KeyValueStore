#include "wal.h"
#include <iostream>

WAL::WAL(const std::string &filename)
{
    file_stream.open(filename, std::ios::in | std::ios::out | std::ios::app | std::ios::binary);

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

std::vector<std::pair<std::string, std::string>> WAL::readAll() {
    std::lock_guard<std::mutex> lock(log_mutex);

    file_stream.clear();
    file_stream.seekg(0, std::ios::beg);

    std::vector<std::pair<std::string, std::string>> results;

    while (file_stream.peek() != EOF) {
        int len = 0;

        file_stream.read(reinterpret_cast<char*>(&len), sizeof(len));

        std::string key(len, '\0');
        file_stream.read(&key[0], len);

        file_stream.read(reinterpret_cast<char*>(&len), sizeof(len));

        std::string value(len, '\0');
        file_stream.read(&value[0], len);

        results.push_back({key, value});
    }

    file_stream.clear();
    file_stream.seekg(0, std::ios::end);

    return results;
}