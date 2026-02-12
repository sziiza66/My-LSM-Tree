#include "lsm_tree.h"

#include <fcntl.h>
#include <unistd.h>
#include <cassert>
#include <cstring>
#include <queue>

#include "sstable/sstable_reader.h"

namespace MyLSMTree {

namespace {

struct TreeParams {
    size_t sstable_scaling_factor;
    size_t memtable_kv_count_limit;
    size_t memtable_kv_count;
    double filter_false_positive_rate;
    size_t bits_count;
    size_t hash_func_count;
    size_t kv_buffer_slice_size;
    size_t fd_cache_size;
    size_t level_count;
};

struct BloomParams {
    size_t bits_count;
    size_t hash_func_count;
};

void ThrowCantOpenTree(const Path& tree_data) {
    throw std::runtime_error(std::string("Can't open tree at ") + tree_data.c_str() + ": " + std::strerror(errno));
}

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

LSMTree::LSMTree(const Path& tree_data) {
    int fd = open(tree_data.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        ThrowCantOpenTree(tree_data);
    }
    TreeParams params;
    ssize_t r = read(fd, &params, sizeof(params));
    if (r != sizeof(params)) {
        ThrowCantOpenTree(tree_data);
    }

    memtable_ =
        std::make_unique<Memtable>(MakeOptimalFilter(params.memtable_kv_count_limit, params.filter_false_positive_rate),
                                   params.memtable_kv_count_limit, params.kv_buffer_slice_size);
    readers_manager_ = std::make_unique<SSTable::SSTableReadersManager>(params.fd_cache_size);
    tree_data_ = tree_data;
    sstable_scaling_factor_ = params.sstable_scaling_factor;
    filter_false_positive_rate_ = params.filter_false_positive_rate;
    memtable_kv_count_limit_ = params.memtable_kv_count_limit;

    levels_.resize(params.level_count);
    read(fd, levels_.data(), levels_.size() * sizeof(levels_[0]));

    Key key;
    Value value;
    for (size_t i = 0; i < params.memtable_kv_count; ++i) {
        KVSizes sizes;
        read(fd, &sizes, sizeof(sizes));
        key.resize(sizes.key_size);
        value.resize(sizes.value_size);
        read(fd, key.data(), key.size() * sizeof(key[0]));
        read(fd, value.data(), value.size() * sizeof(value[0]));
        memtable_->Insert(key, value);
    }

    close(fd);
}

LSMTree::LSMTree(size_t fd_cache_size, size_t sstable_scaling_factor, size_t memtable_kv_count_limit,
                 size_t kv_buffer_slice_size, double filter_false_positive_rate, const Path& tree_data)
    : memtable_(std::make_unique<Memtable>(MakeOptimalFilter(memtable_kv_count_limit, filter_false_positive_rate),
                                           memtable_kv_count_limit, kv_buffer_slice_size)),
      readers_manager_(std::make_unique<SSTable::SSTableReadersManager>(fd_cache_size)),
      tree_data_(tree_data),
      sstable_scaling_factor_(sstable_scaling_factor),
      memtable_kv_count_limit_(memtable_kv_count_limit),
      filter_false_positive_rate_(filter_false_positive_rate) {
}

LSMTree::~LSMTree() noexcept {
    TreeParams params{.sstable_scaling_factor = sstable_scaling_factor_,
                      .memtable_kv_count_limit = memtable_kv_count_limit_,
                      .memtable_kv_count = memtable_->GetKVCount(),
                      .filter_false_positive_rate = filter_false_positive_rate_,
                      .bits_count = memtable_->GetFilterBitsCount(),
                      .hash_func_count = memtable_->GetFilterHashFuncCount(),
                      .kv_buffer_slice_size = memtable_->GetKVBufferSliceSize(),
                      .fd_cache_size = readers_manager_->CacheSize(),
                      .level_count = levels_.size()};
    int fd = open(tree_data_.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        return;
    }
    write(fd, &params, sizeof(params));
    write(fd, levels_.data(), levels_.size() * sizeof(levels_[0]));
    memtable_->DumpKVInFd(fd);
    fsync(fd);
    close(fd);
}

void LSMTree::Insert(const Key& key, const Value& value) {
    const LockGuard lock(mtx_);

    memtable_->Insert(key, value);
    TryCompacting();
}

void LSMTree::Erase(const Key& key) {
    const LockGuard guard(mtx_);

    memtable_->Erase(key);
    TryCompacting();
}

LookupResult LSMTree::Find(const Key& key) const {
    const LockGuard guard(mtx_);

    if (auto res = memtable_->Find(key); res) {
        return res->empty() ? std::nullopt : res;
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

    auto res = memtable_->FindRange(range);
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
    if (memtable_->GetKVCount() < memtable_kv_count_limit_) {
        return;
    }
    auto [number, delete_tombstones] = GetLastComponentAtLevel(0);
    int fd0 = open(GetSSTablePath(0, number).c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd0 < 0) {
        throw std::runtime_error(std::string("Can't create/write sstable with name ") +
                                 GetSSTablePath(0, number).c_str() + ": " + std::strerror(errno));
    }
    size_t true_kv_count = memtable_->MakeSSTableInFd(fd0, delete_tombstones);
    fsync(fd0);
    close(fd0);
    memtable_->Clear();
    if (!true_kv_count) {
        if (delete_tombstones) {
            levels_.pop_back();
        }
        return;
    }
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
    int wfd = open(GetSSTablePath(level, number).c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    size_t components_count = sstable_scaling_factor_ + (level - 1) * (sstable_scaling_factor_ - 1);
    std::vector<SSTableReader> readers;
    readers.reserve(components_count);

    size_t total_kv_count = 0;
    for (size_t i = 0; i < level; ++i) {
        for (size_t j = levels_[i] - 1; ~j; --j) {
            readers.emplace_back(readers_manager_->CreateReader(GetSSTablePath(i, j)));
            total_kv_count += readers.back().GetKVCount();
        }
    }

    std::vector<KVIterator> key_buffer;
    key_buffer.reserve(components_count);
    for (const auto& reader : readers) {
        key_buffer.emplace_back(reader.Begin());
    }
    std::vector<size_t> indexblock;
    Offset kv_offset = 0;
    indexblock.reserve(total_kv_count);
    BloomFilter filter = MakeOptimalFilter(total_kv_count, filter_false_positive_rate_);
    std::vector<size_t> to_advance;
    Value value_buffer;

    auto comparator = [&key_buffer](const size_t& c1, const size_t& c2) {
        int cmp = Compare(key_buffer[c1].GetKey(), key_buffer[c2].GetKey());
        return cmp > 0 || (cmp == 0 && c1 > c2);
    };
    std::priority_queue<size_t, std::vector<size_t>, decltype(comparator)> heap(comparator);
    for (size_t i = 0; i < components_count; ++i) {
        heap.emplace(i);
    }
    while (!heap.empty()) {
        size_t smallest_key_index = heap.top();
        heap.pop();
        const auto& smalles_key = key_buffer[smallest_key_index];
        to_advance.clear();
        to_advance.emplace_back(smallest_key_index);
        while (!heap.empty() && Compare(smalles_key.GetKey(), key_buffer[heap.top()].GetKey()) == 0) {
            to_advance.emplace_back(heap.top());
            heap.pop();
        }

        if (!delete_tombstones || smalles_key.GetValueSize() != 0) {
            value_buffer = smalles_key.GetValue(std::move(value_buffer));
            KVSizes sizes(smalles_key.GetKey().size(), value_buffer.size());
            write(wfd, &sizes, sizeof(sizes));
            write(wfd, smalles_key.GetKey().data(), smalles_key.GetKey().size() * sizeof(smalles_key.GetKey()[0]));
            write(wfd, value_buffer.data(), value_buffer.size() * sizeof(value_buffer[0]));
            filter.Insert(smalles_key.GetKey());
            indexblock.emplace_back(kv_offset);
            size_t kv_size = smalles_key.GetKey().size() + value_buffer.size() + sizeof(sizes);
            kv_offset += kv_size;
        }

        for (size_t i = to_advance.size() - 1; ~i; --i) {
            size_t index = to_advance[i];
            if (key_buffer[index].IsEnd()) {
                continue;
            }
            ++key_buffer[index];
            heap.push(index);
        }
    }
    if (!indexblock.empty()) {
        filter.MakeFilterBlockInFd(wfd);
        write(wfd, indexblock.data(), indexblock.size() * sizeof(indexblock[0]));
        MetaBlock meta(kv_offset, filter.BitsCount(), filter.HashFuncCount(), kv_offset + filter.GetSizeInBytes(),
                    indexblock.size());
        write(wfd, &meta, sizeof(meta));
        fsync(wfd);
        close(wfd);
        ++levels_[level];
    } else if (delete_tombstones) {
        levels_.pop_back();
    }
    for (size_t i = 0; i < level; ++i) {
        levels_[i] = 0;
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
        return {levels_[level], true};
    } else {
        return {levels_[level], false};
    }
}

}  // namespace MyLSMTree
