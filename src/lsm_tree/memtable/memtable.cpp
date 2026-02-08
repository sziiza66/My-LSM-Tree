#include "memtable.h"

namespace MyLSMTree::Memtable {

Memtable::Memtable(size_t kv_count_limit, size_t filter_bits_count, size_t filter_hash_func_count,
                   uint32_t kv_buffer_slice_size, std::mt19937::result_type list_rng_seed)
    : filter_(filter_bits_count, filter_hash_func_count), list_(kv_count_limit, kv_buffer_slice_size, list_rng_seed) {
}

void Memtable::Insert(const uint8_t* kv, uint32_t key_size, uint32_t value_size) {
    filter_.Insert(kv, key_size);
    list_.Insert(kv, key_size, value_size);
}

LookUpResult Memtable::Find(uint8_t* value_dest, const uint8_t* key, uint32_t key_size) const {
    if (!filter_.Find(key, key_size)) {
        return LookUpResult::ValueNotFound;
    }
    return list_.Find(value_dest, key, key_size);
}

LookUpResult Memtable::Find(const uint8_t* key, uint32_t key_size) const {
    if (!filter_.Find(key, key_size)) {
        return LookUpResult::ValueNotFound;
    }
    return list_.Find(key, key_size);
}

void Memtable::Erase(const uint8_t* key, uint32_t key_size) {
    filter_.Insert(key, key_size);
    list_.Erase(key, key_size);
}

void Memtable::Clear() {
    filter_.Clear();
    list_.Clear();
}

size_t Memtable::Size() const {
    return list_.Size();
}

void Memtable::MakeSSTableInFd(int fd) const {
    filter_.MakeFilterBlockInFd(fd);
    list_.MakeIndexAndDataBlocksInFd(fd);
}

}  // namespace MyLSMTree::Memtable
