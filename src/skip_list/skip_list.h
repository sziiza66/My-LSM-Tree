#pragma once
#define SKIPLIST_STATISTICS


#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

#include "kvbuffer.h"

namespace MyLSMTree::Memtable {

class SkipList {
    static constexpr uint32_t kMaxLevel = 32;
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
    SkipList(uint32_t kv_count_limit, uint32_t kv_buffer_slice_size, int dump_fd,
             std::mt19937::result_type rng_seed = 6);

    void Insert(const uint8_t* kv, uint32_t key_size, uint32_t value_size);
    int Find(uint8_t* value_dest, const uint8_t* key, uint32_t key_size) const;
    void Erase(const uint8_t* key, uint32_t key_size);

private:
    uint8_t RandomLevel();
    void WriteToNode(Node& node, const uint8_t* kv, uint32_t key_size, uint32_t value_size);

private:
    std::vector<Node> nodes_;
    std::mt19937 rng_gen_;
    KVBuffer kvbuffer_;
    uint32_t kv_count_limit_;
    uint32_t level_count_limit_;
    uint32_t kv_count = 0;
    int dump_fd_;
#ifdef SKIPLIST_STATISTICS
    uint32_t statistics[kMaxLevel];
#endif
};

}  // namespace MyLSMTree::Memtable
