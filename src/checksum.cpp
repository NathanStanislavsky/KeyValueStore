#include "checksum.h"

#include <array>

namespace {
constexpr uint32_t poly = 0xEDB88320u;

constexpr std::array<uint32_t, 256> make_table()
{
    std::array<uint32_t, 256> table{};

    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j) {
            c = (c & 1u) ? (poly ^ (c >> 1)) : (c >> 1);
        }
        table[i] = c;
    }
    return table;
}

constexpr std::array<uint32_t, 256> crc32_table = make_table();
}

uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    uint32_t c = crc ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        c = crc32_table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

uint32_t crc32(const uint8_t* data, size_t len) {
    return crc32_update(0u, data, len);
}

uint32_t crc32(const std::string& s) {
    return crc32(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}