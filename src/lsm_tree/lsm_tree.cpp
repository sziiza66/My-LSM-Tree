#include "lsm_tree.h"
#include "sstable/sstable_reader.h"

namespace MyLSMTree {

namespace {

struct BloomParams {
    size_t bits_count;
    size_t hash_func_count;
};

BloomParams ComputeBloomParams(size_t key_count, double false_positive_rate) {
    if (key_count == 0) {
        return {0, 0};
    }

    constexpr double ln2 = 0.6931471805599453;
    double bits_count = -static_cast<double>(key_count) * std::logl(false_positive_rate) / (ln2 * ln2);
    double hash_func_count = (bits_count / key_count) * ln2;

    return {static_cast<size_t>(std::ceil(bits_count)),
            static_cast<size_t>(std::max(1.0, std::round(hash_func_count)))};
}

Memtable::BloomFilter MakeOptimalFilter(size_t key_count, double false_positive_rate) {
    auto params = ComputeBloomParams(key_count, false_positive_rate);
    return {params.bits_count, params.hash_func_count};
}

}  // namespace

LSMTree::LSMTree(size_t fd_cache_size, size_t sstable_scaling_factor, size_t memtable_kv_count_limit,
                 size_t kv_buffer_slice_size, double filter_false_positive_rate, const Path& tree_data)
    : memtable_(MakeOptimalFilter(memtable_kv_count_limit, filter_false_positive_rate), memtable_kv_count_limit,
                kv_buffer_slice_size),
      readers_manager_(fd_cache_size),
      tree_data_(tree_data),
      sstable_scaling_factor_(sstable_scaling_factor) {
}

void LSMTree::Insert(const Key& key, const Value& value) {
    memtable_.Insert(key, value);
    TryCompacting();
}

void LSMTree::Erase(const Key& key) {
    memtable_.Erase(key);
    TryCompacting();
}

LookupResult LSMTree::Find(const Key& key) {
    if (auto res = memtable_.Find(key); res) {
        return res;
    }
    if (levels_.empty()) {
        return std::nullopt;
    }

    auto [hash_low, hash_high] = CalculateHash(key.data(), key.size());
    for (size_t i = 0; i < levels_.size(); ++i) {

        for (size_t j = levels_[i].size() - 1; ~j; --j) {
        }
    }
}

Values LSMTree::FindRange(const KeyRange& range) {
    auto res = memtable_.FindRange(range);
}

void LSMTree::TryCompacting() {
    if (memtable_.GetKVCount() < memtable_kv_count_limit_) {
        return;
    }
    memtable_.MakeSSTableInFd(...);
    memtable_.Clear();
    for (size_t i = 0; i < levels_.size() && levels_[i].size() == sstable_scaling_factor_; ++i) {
        CompactIthLevel();
    }
}

void LSMTree::CompactIthLevel() {
}

}  // namespace MyLSMTree
