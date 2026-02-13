#pragma once

#include "bloom_filter/bloom_filter.h"
#include "skip_list/skip_list.h"
#include "../common.h"

namespace MyLSMTree::Memtable {

class Memtable {
public:
    Memtable(size_t filter_bits_count, size_t filter_hash_func_count, size_t kv_count_limit,
             uint32_t kv_buffer_slice_size, std::mt19937::result_type list_rng_seed = 6);

    Memtable(BloomFilter filter, size_t kv_count_limit, uint32_t kv_buffer_slice_size,
             std::mt19937::result_type list_rng_seed = 6);

    void Insert(const Key& key, const Value& value);
    LookupResult Find(const Key& key) const;
    RangeLookupResult FindRange(const KeyRange& range, RangeLookupResult accumulated = {}) const;

    void Erase(const Key& key);
    void Clear();
    size_t GetKVCount() const;
    size_t GetKVBufferSliceSize() const;
    size_t GetFilterBitsCount() const;
    size_t GetFilterHashFuncCount() const;
    size_t MakeSSTableInFd(int fd, bool skip_deleted) const;
    void DumpKVInFd(int fd) const;

private:
    BloomFilter filter_;
    SkipList list_;
};

}  // namespace MyLSMTree::Memtable
