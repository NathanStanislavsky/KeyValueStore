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
