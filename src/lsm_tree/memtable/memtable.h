#pragma once

#include "bloom_filter/bloom_filter.h"
#include "skip_list/skip_list.h"
#include "../common_types.h"

namespace MyLSMTree::Memtable {

class Memtable {
public:
    Memtable(size_t kv_count_limit, size_t filter_bits_count, size_t filter_hash_func_count,
             uint32_t kv_buffer_slice_size, std::mt19937::result_type list_rng_seed = 6);

    void Insert(const uint8_t* kv, uint32_t key_size, uint32_t value_size);
    LookUpResult Find(uint8_t* value_dest, const uint8_t* key, uint32_t key_size) const;
    LookUpResult Find(const uint8_t* key, uint32_t key_size) const;
    void Erase(const uint8_t* key, uint32_t key_size);
    void Clear();
    size_t Size() const;
    void MakeSSTableInFd(int fd) const;

private:
    BloomFilter filter_;
    SkipList list_;
};

}  // namespace MyLSMTree::Memtable
