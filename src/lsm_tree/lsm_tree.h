#pragma once

#include "common.h"
#include "memtable/memtable.h"
#include "sstable/sstable_reader.h"

namespace MyLSMTree {

class LSMTree {
    using Memtable = Memtable::Memtable;
    using SSTableReadersManager = SSTable::SSTableReadersManager;
    using Level = std::vector<Path>;
    using Levels = std::vector<Level>;

public:
    LSMTree(size_t fd_cache_size, size_t sstable_scaling_factor, size_t memtable_kv_count_limit, size_t kv_buffer_slice_size,
            double filter_false_positive_rate, const Path& tree_data);

    void Insert(const Key& key, const Value& value);
    void Erase(const Key& key);
    LookupResult Find(const Key& key);
    Values FindRange(const KeyRange& range);

private:
    void TryCompacting();
    void CompactIthLevel();

private:
    Memtable memtable_;
    SSTable::SSTableReadersManager readers_manager_;
    Levels levels_;
    Path tree_data_;
    size_t sstable_scaling_factor_;
    size_t memtable_kv_count_limit_;
};

}  // namespace MyLSMTree
