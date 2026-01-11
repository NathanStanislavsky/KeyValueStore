#pragma once
#include "sstable.h"
#include <fstream>

class SSTableIterator {
    public:
        SSTableIterator(const std::string& filename, int fileId);

        void next();
        
        bool hasNext();

        std::string key() const;
        std::string value() const;
        int getFileId() const;

    private:
        std::ifstream file;
        std::string current_key;
        std::string current_value;
        int file_id;
        bool is_valid;
};