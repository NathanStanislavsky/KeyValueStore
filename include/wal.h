#pragma once
#include <string>
#include <fstream>
#include <mutex>

class WAL {
    public:
        WAL(const std::string& filename);

        ~WAL();

        bool write(const std::string& key, const std::string& value);

    private:
        std::fstream file_stream;
        std::mutex log_mutex;
};