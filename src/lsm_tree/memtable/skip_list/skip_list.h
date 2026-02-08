#pragma once
// #define SKIPLIST_STATISTICS

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

#include "kvbuffer.h"
#include "../../common_types.h"

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

    void Insert(const uint8_t* kv, uint32_t key_size, uint32_t value_size);
    LookUpResult Find(uint8_t* value_dest, const uint8_t* key, uint32_t key_size) const;
    LookUpResult Find(const uint8_t* key, uint32_t key_size) const;
    void Erase(const uint8_t* key, uint32_t key_size);
    void Clear();
    size_t Size() const;
    void MakeIndexAndDataBlocksInFd(int fd) const;

private:
    uint8_t RandomLevel();
    void WriteToNode(Node& node, const uint8_t* kv, uint32_t key_size, uint32_t value_size);

    void MakeIndexBlockInFd(int fd) const;
    void MakeDataBlockInFd(int fd) const;

private:
    std::vector<Node> nodes_;
    mutable std::vector<Index> index_block_buffer_;
    std::mt19937 rng_gen_;
    KVBuffer kvbuffer_;
    size_t level_count_limit_;
    size_t kv_count_ = 0;
#ifdef SKIPLIST_STATISTICS
    uint32_t statistics[kMaxLevel];
#endif
};

}  // namespace MyLSMTree::Memtable
