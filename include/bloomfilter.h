#pragma once
#include <vector>
#include <string>

class BloomFilter
{
public:
    BloomFilter(size_t numKeys, int k = 3);

    void add(const std::string &key);

    bool contains(const std::string &key) const;

private:
    std::vector<bool> bits;

    std::vector<size_t> getHashIndices(const std::string &key) const;

    int k;
};