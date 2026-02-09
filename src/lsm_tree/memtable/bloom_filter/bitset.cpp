#include "bitset.h"
#include <cstdint>

namespace MyLSMTree::Memtable {

Bitset::Bitset(size_t bits_count) : data_((bits_count + 63) / 64, 0) {
}

bool Bitset::Test(size_t i) const {
    return data_[i >> 6] & (1ULL << (i & 63));
}

void Bitset::Set(size_t i) {
    data_[i >> 6] |= (1ULL << (i & 63));
}

void Bitset::Reset(size_t i) {
    data_[i >> 6] &= ~(1ULL << (i & 63));
}

void Bitset::Clear() {
    data_.assign(data_.size(), 0);
}

const uint64_t* Bitset::Data() const {
    return data_.data();
}

size_t Bitset::Size() const {
    return data_.size() * 8;
}

}  // namespace MyLSMTree::Memtable
