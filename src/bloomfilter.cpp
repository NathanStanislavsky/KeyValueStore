#include "bloomfilter.h"
#include <functional>

BloomFilter::BloomFilter(size_t numKeys, int k) : k(k) {

    if (numKeys <= 0) {
        throw std::invalid_argument("numKeys must be greater than 0");
    }

    bits.resize(numKeys * 10);
}

std::vector<size_t> BloomFilter::getHashIndices(const std::string& key) const {
    std::vector<size_t> indices;

    size_t hash1 = std::hash<std::string>()(key);
    size_t hash2 = hash1 * 0x9e3779b9;

    for (int i = 0; i < k; i++) {
        size_t index = (hash1 + i * hash2) % bits.size();
        indices.push_back(index);
    }

    return indices;
}

void BloomFilter::add(const std::string& key) {
    std::vector<size_t> indices = getHashIndices(key);

    for (size_t index : indices) {
        bits[index] = true;
    }
}

bool BloomFilter::contains(const std::string& key) const {
    std::vector<size_t> indices = getHashIndices(key);

    for (size_t index : indices) {
        if (!bits[index]) {
            return false;
        }
    }

    return true;
}
