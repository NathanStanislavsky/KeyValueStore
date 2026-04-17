#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <utility>
#include <cstdint>

#pragma pack(push, 1)
struct WALRecordHeader 
{
    uint32_t magic;
    uint8_t version;
    uint8_t flags;
    uint16_t reserved;
    uint32_t key_len;
    uint32_t value_len;
    uint32_t checksum;
};
#pragma pack(pop)

class WAL
{
public:
    WAL(const std::string &filename);

    ~WAL();

    bool write(const std::string &key, const std::string &value);

    std::vector<std::pair<std::string, std::string>> readAll();

    static std::vector<std::pair<std::string, std::string>> readAllFromFile(const std::string &path);

    void clear();

    void rotate();

    void clearTemp();

private:
    std::fstream file_stream;
    std::mutex log_mutex;
    std::string filename;
};