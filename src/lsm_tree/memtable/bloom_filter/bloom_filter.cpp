#include "bloom_filter.h"
#include <unistd.h>

#include <cassert>
#include <type_traits>

#include "../xxhash/xxhash.h"

namespace MyLSMTree::Memtable {

namespace {

uint64_t CalculateIthHash(const uint8_t* data, size_t size, size_t i, size_t mod) {
    static_assert(std::is_same_v<XXH64_hash_t, uint64_t>);
    XXH128_hash_t h128 = XXH3_128bits(data, size);
    return (h128.low64 + i * h128.low64) % mod;
}

}  // namespace

BloomFilter::BloomFilter(size_t bits_count, size_t hash_func_count)
    : filter_(bits_count), hash_func_count_(hash_func_count) {
    assert(bits_count > 0);
    assert(hash_func_count > 0);
}

void BloomFilter::Insert(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < hash_func_count_; ++i) {
        filter_.Set(CalculateIthHash(data, size, i, filter_.SizeInBits()));
    }
}

bool BloomFilter::Find(const uint8_t* data, size_t size) const {
    for (size_t i = 0; i < hash_func_count_; ++i) {
        if (!filter_.Test(CalculateIthHash(data, size, i, filter_.SizeInBits()))) {
            return false;
        }
    }
    return true;
}

void BloomFilter::MakeFilterBlockInFd(int fd) const {
    write(fd, filter_.Data(), filter_.SizeInBytes());
}

void BloomFilter::Clear() {
    filter_.Clear();
}

}  // namespace MyLSMTree::Memtable
