#include "memtable.h"

namespace MyLSMTree::Memtable {

Memtable::Memtable(size_t filter_bits_count, size_t filter_hash_func_count, size_t kv_count_limit,
                   uint32_t kv_buffer_slice_size, std::mt19937::result_type list_rng_seed)
    : filter_(filter_bits_count, filter_hash_func_count), list_(kv_count_limit, kv_buffer_slice_size, list_rng_seed) {
}

Memtable::Memtable(BloomFilter filter, size_t kv_count_limit, uint32_t kv_buffer_slice_size,
                   std::mt19937::result_type list_rng_seed)
    : filter_(std::move(filter)), list_(kv_count_limit, kv_buffer_slice_size, list_rng_seed) {
}

void Memtable::Insert(const Key& key, const Value& value) {
    filter_.Insert(key);
    list_.Insert(key, value);
}

LookupResult Memtable::Find(const Key& key) const {
    // if (!filter_.Find(key.data(), key.size())) {
    //     return std::nullopt;
    // }
    return list_.Find(key);
}

RangeLookupResult Memtable::FindRange(const KeyRange& range, RangeLookupResult accumulated) const {
    return list_.FindRange(range, std::move(accumulated));
}

void Memtable::Erase(const Key& key) {
    filter_.Insert(key);
    list_.Erase(key);
}

void Memtable::Clear() {
    filter_.Clear();
    list_.Clear();
}

size_t Memtable::GetKVCount() const {
    return list_.Size();
}

size_t Memtable::GetKVBufferSliceSize() const {
    return list_.GetKVBufferSliceSize();
}

size_t Memtable::GetFilterBitsCount() const {
    return filter_.BitsCount();
}

size_t Memtable::GetFilterHashFuncCount() const {
    return filter_.HashFuncCount();
}

size_t Memtable::MakeSSTableInFd(int fd, bool skip_deleted) const {
    auto [true_kv_count, true_data_size_in_bytes] = list_.MakeDataBlockInFd(fd, skip_deleted);
    if (!true_kv_count) {
        return true_kv_count;
    }
    filter_.MakeFilterBlockInFd(fd);
    list_.MakeIndexBlockInFd(fd, skip_deleted);

    Offset filter_offset = true_data_size_in_bytes + true_kv_count * sizeof(KVSizes);
    MetaBlock meta{.filter_offset = filter_offset,
                   .filter_bits_count = filter_.BitsCount(),
                   .filter_hash_func_count = filter_.HashFuncCount(),
                   .index_offset = filter_offset + filter_.GetSizeInBytes(),
                   .kv_count = true_kv_count};
    write(fd, &meta, sizeof(meta));
    return true_kv_count;
}

void Memtable::DumpKVInFd(int fd) const {
    list_.MakeDataBlockInFd(fd, false);
}


}  // namespace MyLSMTree::Memtable
