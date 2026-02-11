#pragma once

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

#include "kvbuffer.h"
#include "../../common.h"

namespace MyLSMTree::Memtable {

class SkipList {
    static constexpr size_t kMaxLevel = 32;
    static constexpr uint32_t kNil = -1;

    struct Node {
        uint32_t next[kMaxLevel];
        size_t key_offset;
        uint32_t key_size;
        uint32_t value_size;
        uint8_t height = 0;

        Node() {
            std::fill_n(next, kMaxLevel, kNil);
        }
    };

public:
    SkipList(size_t kv_count_limit, uint32_t kv_buffer_slice_size, std::mt19937::result_type rng_seed = 6);

    void Insert(const Key& key, const Value& value);
    void Erase(const Key& key);
    LookupResult Find(const Key& key) const;
    IncompleteRangeLookupResult FindRange(const KeyRange& range) const;
    void Clear();
    size_t Size() const;
    size_t GetDataSizeInBytes() const;
    size_t GetKVBufferSliceSize() const;
    void MakeIndexBlockInFd(int fd, bool skip_deleted) const;
    std::pair<size_t, size_t> MakeDataBlockInFd(int fd, bool skip_deleted) const;

private:
    uint32_t FindNode(const Key& key, bool including) const;
    int Compare(uint32_t node_index, const Key& key) const;

    uint8_t RandomLevel();
    void WriteToNode(Node& node, const Key& key, const Value& value);

private:
    std::vector<Node> nodes_;
    mutable std::vector<Offset> index_block_buffer_;
    std::mt19937 rng_gen_;
    KVBuffer kvbuffer_;
    size_t level_count_limit_;
    size_t kv_count_ = 0;
#ifndef NDEBUG
    uint32_t statistics[kMaxLevel];
#endif
};

}  // namespace MyLSMTree::Memtable
