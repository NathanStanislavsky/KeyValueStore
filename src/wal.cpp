#include "wal.h"
#include "checksum.h"
#include <iostream>

WAL::WAL(const std::string &filename) : filename(filename)
{
    file_stream.open(filename, std::ios::in | std::ios::out | std::ios::app | std::ios::binary);

    if (!file_stream.is_open())
    {
        std::cerr << "Failed to open WAL file: " << filename << std::endl;
    }
}

WAL::~WAL()
{
    if (file_stream.is_open())
    {
        file_stream.close();
    }
}

bool WAL::write(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(log_mutex);

    WALRecordHeader header;

    header.magic = 0xDEADBEEF;
    header.version = 1;
    header.flags = 0;
    header.reserved = 0;
    header.key_len = key.size();
    header.value_len = value.size();

    const uint8_t* header_bytes = reinterpret_cast<const uint8_t*>(&header);

    uint32_t crc = crc32_update(0u, header_bytes, 16);

    crc = crc32_update(crc, reinterpret_cast<const uint8_t*>(key.data()), key.size());
    crc = crc32_update(crc, reinterpret_cast<const uint8_t*>(value.data()), value.size());

    header.checksum = crc ^ 0xFFFFFFFFu;

    file_stream.write(reinterpret_cast<const char *>(&header), sizeof(WALRecordHeader));

    file_stream.write(key.data(), key.size());
    file_stream.write(value.data(), value.size());

    file_stream.flush();

    return file_stream.good();
}

std::vector<std::pair<std::string, std::string>> WAL::readAll()
{
    std::lock_guard<std::mutex> lock(log_mutex);

    file_stream.clear();
    file_stream.seekg(0, std::ios::beg);

    std::vector<std::pair<std::string, std::string>> results;

    while (file_stream.peek() != EOF)
    {
        int len = 0;

        file_stream.read(reinterpret_cast<char *>(&len), sizeof(len));

        std::string key(len, '\0');
        file_stream.read(&key[0], len);

        file_stream.read(reinterpret_cast<char *>(&len), sizeof(len));

        std::string value(len, '\0');
        file_stream.read(&value[0], len);

        results.push_back({key, value});
    }

    file_stream.clear();
    file_stream.seekg(0, std::ios::end);

    return results;
}

void WAL::clear()
{
    std::lock_guard<std::mutex> lock(log_mutex);

    file_stream.close();

    file_stream.open(filename, std::ios::out | std::ios::trunc);
    file_stream.close();

    file_stream.open(filename, std::ios::in | std::ios::out | std::ios::app | std::ios::binary);
}

void WAL::rotate()
{
    std::lock_guard<std::mutex> lock(log_mutex);
    
    file_stream.close();
    
    std::string temp_name = filename + ".tmp";
    std::rename(filename.c_str(), temp_name.c_str());
    
    file_stream.open(filename, std::ios::in | std::ios::out | std::ios::app | std::ios::binary);
    
    if (!file_stream.is_open())
    {
        std::cerr << "Failed to reopen WAL file after rotation: " << filename << std::endl;
    }
}

void WAL::clearTemp()
{
    std::string temp_name = filename + ".tmp";
    std::remove(temp_name.c_str());
}