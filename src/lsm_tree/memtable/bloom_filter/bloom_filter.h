#pragma once

#include "bitset.h"

namespace MyLSMTree::Memtable {

class BloomFilter {
public:
    BloomFilter(size_t bits_count, size_t hash_func_count);

    void Insert(const uint8_t* data, size_t size);
    bool Find(const uint8_t* data, size_t size) const;
    void MakeFilterBlockInFd(int fd) const;
    void Clear();

private:
    Bitset filter_;
    size_t hash_func_count_;
};

}  // namespace MyLSMTree::Memtable
