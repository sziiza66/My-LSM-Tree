#include "common.h"

#include <cstring>
#include <type_traits>

#include "xxhash.h"

namespace MyLSMTree {

std::pair<uint64_t, uint64_t> CalculateHash(const uint8_t* data, size_t size) {
    static_assert(std::is_same_v<XXH64_hash_t, uint64_t>);
    auto hash = XXH3_128bits(data, size);
    return {hash.low64, hash.high64};
}

uint64_t CalculateIthHash(uint64_t low64, uint64_t high64, size_t i, size_t mod) {
    return (low64 + i * high64) % mod;
}

uint64_t CalculateIthHash(const uint8_t* data, size_t size, size_t i, size_t mod) {
    auto h128 = CalculateHash(data, size);
    return CalculateIthHash(h128.first, h128.second, i, mod);
}

int Compare(const Key& lhs, const Key& rhs) {
    int cmp = std::memcmp(lhs.data(), rhs.data(), lhs.size() < rhs.size() ? lhs.size() : rhs.size());
    return cmp == 0 ? lhs.size() < rhs.size() ? -1 : lhs.size() > rhs.size() ? 1 : 0 : cmp;
}

std::vector<uint8_t> ToBytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

}  // namespace MyLSMTree
