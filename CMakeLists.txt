cmake_minimum_required(VERSION 3.10)

project(kv-server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(SOURCES 
    src/main.cpp
    src/wal.cpp
    src/memtable.cpp
    src/kvstore.cpp
    src/sstable.cpp
    src/bloomfilter.cpp
    src/SSTableIterator.cpp
)

add_executable(kv-server ${SOURCES})

target_include_directories(kv-server PRIVATE include)