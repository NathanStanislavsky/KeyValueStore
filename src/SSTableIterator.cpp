#include "SSTableIterator.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

SSTableIterator::SSTableIterator(const std::string &filename, int fileId)
{
    file_id = fileId;
    is_valid = false;

    if (!fs::exists(filename))
    {
        return;
    }

    file.open(filename, std::ios::binary);

    if (!file.is_open())
    {
        std::cerr << "Failed to open SSTable file: " << filename << std::endl;
        return;
    }

    next();
}

void SSTableIterator::next()
{
    int key_len = 0;

    if (!file.read(reinterpret_cast<char *>(&key_len), sizeof(key_len)))
    {
        is_valid = false;
        return;
    }

    current_key.resize(key_len);
    file.read(&current_key[0], key_len);

    int value_len = 0;
    file.read(reinterpret_cast<char *>(&value_len), sizeof(value_len));

    current_value.resize(value_len);
    file.read(&current_value[0], value_len);

    is_valid = true;
}

bool SSTableIterator::hasNext()
{
    return is_valid;
}

std::string SSTableIterator::key() const
{
    return current_key;
}

std::string SSTableIterator::value() const
{
    return current_value;
}

int SSTableIterator::getFileId() const
{
    return file_id;
}