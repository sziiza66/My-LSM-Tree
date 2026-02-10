#include "lsm_tree.h"

#include <fcntl.h>
#include <unistd.h>
#include <cassert>

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
      readers_manager_(std::make_unique<SSTable::SSTableReadersManager>(fd_cache_size)),
      tree_data_(tree_data),
      sstable_scaling_factor_(sstable_scaling_factor),
      filter_false_positive_rate_(filter_false_positive_rate) {
}

void LSMTree::Insert(const Key& key, const Value& value) {
    const LockGuard lock(mtx_);

    memtable_.Insert(key, value);
    TryCompacting();
}

void LSMTree::Erase(const Key& key) {
    const LockGuard guard(mtx_);

    memtable_.Erase(key);
    TryCompacting();
}

LookupResult LSMTree::Find(const Key& key) const {
    const LockGuard guard(mtx_);

    if (auto res = memtable_.Find(key); res) {
        return res;
    }
    if (levels_.empty()) {
        return std::nullopt;
    }

    auto [hash_low, hash_high] = CalculateHash(key.data(), key.size());
    Key buffer;
    for (size_t i = 0; i < levels_.size(); ++i) {
        for (size_t j = levels_[i] - 1; ~j; --j) {
            auto reader = readers_manager_->CreateReader(GetSSTablePath(i, j));
            if (!reader.TestHashes(hash_low, hash_high)) {
                continue;
            }
            auto [value, buffer_ret] = reader.Find(key, std::move(buffer));
            if (value) {
                return value->empty() ? std::nullopt : value;
            }
            buffer = std::move(buffer_ret);
        }
    }

    return std::nullopt;
}

RangeLookupResult LSMTree::FindRange(const KeyRange& range) const {
    const LockGuard guard(mtx_);

    auto res = memtable_.FindRange(range);
    Key buffer;
    for (size_t i = 0; i < levels_.size(); ++i) {
        for (size_t j = levels_[i] - 1; ~j; --j) {
            auto reader = readers_manager_->CreateReader(GetSSTablePath(i, j));
            auto [res_ret, buffer_ret] = reader.FindRange(range, std::move(res), std::move(buffer));
            res = std::move(res_ret);
            buffer = std::move(buffer_ret);
        }
    }

    return res.accumutaled;
}

void LSMTree::TryCompacting() {
    if (memtable_.GetKVCount() < memtable_kv_count_limit_) {
        return;
    }
    auto [number, delete_tombstones] = GetLastComponentAtLevel(0);
    int fd0 = open(GetSSTablePath(0, number).c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC);
    if (fd0 < 0) {
        throw std::runtime_error(std::string("Can't create/open sstable with name ") +
                                 GetSSTablePath(0, number).c_str());
    }
    memtable_.MakeSSTableInFd(fd0, delete_tombstones);
    fsync(fd0);
    memtable_.Clear();
    ++levels_[0];
    if (levels_[0] == sstable_scaling_factor_) {
        size_t i = 1;
        while (i < levels_.size() && levels_[i] + 1 == sstable_scaling_factor_) {
            ++i;
        }
        CompactLevelsUpTo(i);
    }
}

void LSMTree::CompactLevelsUpTo(size_t level) {
    auto [number, delete_tombstones] = GetLastComponentAtLevel(level);
    int wfd = open(GetSSTablePath(level, number).c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC);
    size_t components_count = sstable_scaling_factor_ + (level - 1) * (sstable_scaling_factor_ - 1);
    std::vector<SSTableReader> readers;
    readers.reserve(components_count);
    std::vector<size_t> carets(components_count, 0);

    size_t total_kv_count = 0;
    for (size_t i = 0; i < level; ++i) {
        for (size_t j = levels_[i] - 1; ~j; --j) {
            readers.emplace_back(readers_manager_->CreateReader(GetSSTablePath(i, j)));
            total_kv_count += readers.back().GetKVCount();
        }
    }

    std::vector<Key> key_buffer(components_count);
    BloomFilter filter = MakeOptimalFilter(total_kv_count, filter_false_positive_rate_);
    MetaBlock meta(0, filter.BitsCount(), filter.HashFuncCount(), filter.GetSizeInBytes(), total_kv_count);
    while (total_kv_count--) {

    }
}

size_t LSMTree::CalculateKVCountForLevel(size_t level) const {
    size_t count = memtable_kv_count_limit_;
    while (level--) {
        count *= sstable_scaling_factor_;
    }
    return count;
}

Path LSMTree::GetSSTablePath(size_t level, size_t number) const {
    return std::to_string(level) + '_' + std::to_string(number) + ".sst";
}

LSMTree::ComponentInfo LSMTree::GetLastComponentAtLevel(size_t level) {
    assert(level <= levels_.size());
    if (level == levels_.size()) {
        levels_.emplace_back(0);
        return {levels_[level] - 1, true};
    } else {
        return {levels_[level] - 1, false};
    }
}

}  // namespace MyLSMTree
