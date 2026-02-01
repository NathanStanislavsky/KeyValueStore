#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <utility>

class WAL
{
public:
    WAL(const std::string &filename);

    ~WAL();

    bool write(const std::string &key, const std::string &value);

    std::vector<std::pair<std::string, std::string>> readAll();

    void clear();

    void rotate();

    void clearTemp();

private:
    std::fstream file_stream;
    std::mutex log_mutex;
    std::string filename;
};