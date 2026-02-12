#include "bloom_filter.h"

#include <cassert>
#include <unistd.h>

#include "../../common.h"

namespace MyLSMTree::Memtable {

BloomFilter::BloomFilter(size_t bits_count, size_t hash_func_count)
    : filter_(bits_count), hash_func_count_(hash_func_count), bits_count_(bits_count) {
}

BloomFilter::BloomFilter(Bitset filter, size_t bits_count, size_t hash_func_count)
    : filter_(std::move(filter)), hash_func_count_(hash_func_count), bits_count_(bits_count) {
}


void BloomFilter::Insert(const Key& key) {
    Insert(key.data(), key.size() * sizeof(key[0]));
}

bool BloomFilter::Find(const Key& key) {
    return Find(key.data(), key.size() * sizeof(key[0]));
}


void BloomFilter::MakeFilterBlockInFd(int fd) const {
    write(fd, filter_.Data(), filter_.GetSizeInBytes());
}

void BloomFilter::Clear() {
    filter_.Clear();
}

size_t BloomFilter::BitsCount() const {
    return bits_count_;
}

size_t BloomFilter::HashFuncCount() const {
    return hash_func_count_;
}

size_t BloomFilter::GetSizeInBytes() const {
    return filter_.GetSizeInBytes();
}

void BloomFilter::Insert(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < hash_func_count_; ++i) {
        filter_.Set(CalculateIthHash(data, size, i, bits_count_));
    }
}

bool BloomFilter::Find(const uint8_t* data, size_t size) const {
    for (size_t i = 0; i < hash_func_count_; ++i) {
        if (!filter_.Test(CalculateIthHash(data, size, i, bits_count_))) {
            return false;
        }
    }
    return true;
}

}  // namespace MyLSMTree::Memtable
