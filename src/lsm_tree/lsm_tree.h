#pragma once

#include <memory>
#include <mutex>
#include "common.h"
#include "memtable/memtable.h"
#include "sstable/sstable_reader.h"

namespace MyLSMTree {

class LSMTree {
    using Memtable = Memtable::Memtable;
    using BloomFilter = MyLSMTree::Memtable::BloomFilter;
    using SSTableReadersManager = SSTable::SSTableReadersManager;
    using SSTableReader = SSTableReadersManager::SSTableReader;
    using Level = std::vector<Path>;
    using Levels = std::vector<size_t>;
    using LockGuard = std::lock_guard<std::mutex>;
    using KeyWithValueToken = SSTableReader::KeyWithValueToken;

    struct ComponentInfo {
        size_t number;
        bool level_was_created_just_now;
    };

public:
    explicit LSMTree(const Path& tree_data);
    LSMTree(size_t fd_cache_size, size_t sstable_scaling_factor, size_t memtable_kv_count_limit,
            size_t kv_buffer_slice_size, double filter_false_positive_rate, const Path& tree_data);
    ~LSMTree() noexcept;

    void Insert(const Key& key, const Value& value);
    void Erase(const Key& key);
    LookupResult Find(const Key& key) const;
    RangeLookupResult FindRange(const KeyRange& range) const;

private:
    void TryCompacting();
    void CompactLevelsUpTo(size_t level);
    size_t CalculateKVCountForLevel(size_t level) const;
    Path GetSSTablePath(size_t level, size_t number) const;
    ComponentInfo GetLastComponentAtLevel(size_t level);

private:
    std::unique_ptr<Memtable> memtable_;
    std::unique_ptr<SSTable::SSTableReadersManager> readers_manager_;
    Levels levels_;
    Path tree_data_;
    size_t sstable_scaling_factor_;
    size_t memtable_kv_count_limit_;
    double filter_false_positive_rate_;
    mutable std::mutex mtx_;
};

}  // namespace MyLSMTree
