#pragma once

#include "../../common.h"

#include "bitset.h"

namespace MyLSMTree::Memtable {

class BloomFilter {
public:
    BloomFilter(size_t bits_count, size_t hash_func_count);
    BloomFilter(Bitset filter, size_t hash_func_count, size_t bits_count);

    void Insert(const Key& key);
    bool Find(const Key& key);
    void MakeFilterBlockInFd(int fd) const;
    void Clear();
    size_t BitsCount() const;
    size_t HashFuncCount() const;
    size_t GetSizeInBytes() const;

private:
    bool Find(const uint8_t* data, size_t size) const;
    void Insert(const uint8_t* data, size_t size);

private:
    Bitset filter_;
    size_t hash_func_count_;
    size_t bits_count_;
};

}  // namespace MyLSMTree::Memtable
