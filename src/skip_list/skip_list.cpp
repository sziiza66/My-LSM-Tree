#include "skip_list.h"

#include <bit>
#include <stdexcept>

namespace MyLSMTree::Memtable {

namespace {

uint32_t ThrowIfZeroLimit() {
    throw std::runtime_error("Skiplsit must have kv_count_limit > 0.");
}

}  // namespace

SkipList::SkipList(uint32_t kv_count_limit, uint32_t kv_buffer_slice_size, int dump_fd,
                   std::mt19937::result_type rng_seed)
    : rng_gen_(rng_seed),
      kvbuffer_(kv_buffer_slice_size),
      kv_count_limit_(kv_count_limit),
      level_count_limit_(kv_count_limit
                             ? std::min(kMaxLevel, static_cast<uint32_t>(std::bit_width(kv_count_limit) + 3))
                             : ThrowIfZeroLimit()),
      dump_fd_(dump_fd) {
    nodes_.reserve(kv_count_limit * level_count_limit_ + 1);
    nodes_.emplace_back();
#ifdef SKIPLIST_STATISTICS
    std::fill_n(statistics, kMaxLevel, 0);
#endif
}

int SkipList::Find(uint8_t* value_dest, const uint8_t* key, uint32_t key_size) const {
    if (!kv_count) {
        return 0;
    }
    auto cur_node = 0;
    for (uint32_t cur_level = level_count_limit_ - 1; ~cur_level; --cur_level) {
        while (true) {
            uint32_t next_node = nodes_[cur_node].next[cur_level];
            int cmp =
                next_node == kNil
                    ? -1
                    : kvbuffer_.Compare(key, nodes_[next_node].key_offset,
                                        key_size < nodes_[next_node].key_size ? key_size : nodes_[next_node].key_size);
            cmp = cmp != 0 ? cmp : (key_size < nodes_[next_node].key_size ? -1 : (key_size > nodes_[next_node].key_size ? 1 : 0));
            if (cmp == 0) {
                const auto& node = nodes_[next_node];
                if (node.value_size == 0) {
                    return -1;
                }
                kvbuffer_.Write(value_dest, node.key_offset + node.key_size, node.value_size);
                return 1;
            } else if (cmp < 0) {
                break;
            }
            cur_node = next_node;
        }
    }
    return 0;
}

void SkipList::Insert(const uint8_t* kv, uint32_t key_size, uint32_t value_size) {
    if (!kv_count) {
        ++kv_count;
        nodes_.emplace_back();
        auto& new_node = nodes_.back();
        new_node.height = RandomLevel();
        for (uint32_t level = 0; level < new_node.height; ++level) {
            nodes_[0].next[level] = 1;
        }
        WriteToNode(new_node, kv, key_size, value_size);
    } else {
        uint32_t update[kMaxLevel];
        std::fill_n(update, kMaxLevel, 0);
        auto cur_node = 0;
        for (uint32_t cur_level = level_count_limit_ - 1; ~cur_level; --cur_level) {
            while (true) {
                uint32_t next_node = nodes_[cur_node].next[cur_level];
                int cmp = next_node == kNil
                              ? -1
                              : kvbuffer_.Compare(
                                    kv, nodes_[next_node].key_offset,
                                    key_size < nodes_[next_node].key_size ? key_size : nodes_[next_node].key_size);
                cmp = cmp != 0 ? cmp : (key_size < nodes_[next_node].key_size ? -1 : (key_size > nodes_[next_node].key_size ? 1 : 0));
                if (cmp == 0) {
                    if (value_size == 0) {
                        nodes_[next_node].value_size = 0;
                    } else {
                        WriteToNode(nodes_[next_node], kv, key_size, value_size);
                    }
                    return;
                } else if (cmp < 0) {
                    break;
                }
                cur_node = next_node;
            }
            update[cur_level] = cur_node;
        }
        nodes_.emplace_back();
        auto& new_node = nodes_.back();
        new_node.height = RandomLevel();
        for (uint32_t level = 0; level < new_node.height; ++level) {
            auto& prev_node = nodes_[update[level]];
            new_node.next[level] = prev_node.next[level];
            prev_node.next[level] = nodes_.size() - 1;
        }
        WriteToNode(nodes_.back(), kv, key_size, value_size);
        ++kv_count;
    }

    if (kv_count == kv_count_limit_) {
        DumpAllToFd();
    }
}

void SkipList::Erase(const uint8_t* key, uint32_t key_size) {
    Insert(key, key_size, 0);
}

uint8_t SkipList::RandomLevel() {
    uint8_t level = 0;
    while (level < level_count_limit_ && rng_gen_() % 2) {
        ++level;
    }
#ifdef SKIPLIST_STATISTICS
    ++statistics[level];
#endif
    return level + 1;
}

void SkipList::WriteToNode(Node& node, const uint8_t* kv, uint32_t key_size, uint32_t value_size) {
    node.key_offset = kvbuffer_.GetTotalKVSizeInBytes();
    node.key_size = key_size;
    node.value_size = value_size;
    kvbuffer_.Append(kv, key_size + value_size);
}

void SkipList::DumpAllToFd() {
    // TODO
}

}  // namespace MyLSMTree::Memtable
