#include "skip_list.h"

#include <bit>
#include <stdexcept>

namespace MyLSMTree::Memtable {

namespace {

size_t ThrowIfZeroLimit() {
    throw std::runtime_error("Skiplsit must have kv_count_limit > 0.");
}

}  // namespace

SkipList::SkipList(size_t kv_count_limit, uint32_t kv_buffer_slice_size, std::mt19937::result_type rng_seed)
    : index_block_buffer_(kv_count_limit),
      rng_gen_(rng_seed),
      kvbuffer_(kv_buffer_slice_size),
      level_count_limit_(kv_count_limit ? std::min(kMaxLevel, static_cast<size_t>(std::bit_width(kv_count_limit) + 3))
                                        : ThrowIfZeroLimit()) {
    nodes_.reserve(kv_count_limit * level_count_limit_ + 1);
    nodes_.emplace_back();
#ifndef NDEBUG
    std::fill_n(statistics, kMaxLevel, 0);
#endif
}

void SkipList::Insert(const Key& key, const Value& value) {
    if (!kv_count_) {
        ++kv_count_;
        nodes_.emplace_back();
        auto& new_node = nodes_.back();
        new_node.height = RandomLevel();
        for (size_t level = 0; level < new_node.height; ++level) {
            nodes_[0].next[level] = 1;
        }
        WriteToNode(new_node, key, value);
    } else {
        size_t update[kMaxLevel];
        std::fill_n(update, kMaxLevel, 0);
        auto cur_node = 0;
        for (size_t cur_level = level_count_limit_ - 1; ~cur_level; --cur_level) {
            while (true) {
                size_t next_node = nodes_[cur_node].next[cur_level];
                int cmp = Compare(next_node, key);
                if (cmp == 0) {
                    if (value.size() == 0) {
                        nodes_[next_node].value_size = 0;
                    } else {
                        WriteToNode(nodes_[next_node], key, value);
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
        for (size_t level = 0; level < new_node.height; ++level) {
            auto& prev_node = nodes_[update[level]];
            new_node.next[level] = prev_node.next[level];
            prev_node.next[level] = nodes_.size() - 1;
        }
        WriteToNode(nodes_.back(), key, value);
        ++kv_count_;
    }
}

void SkipList::Erase(const Key& key) {
    Insert(key, {});
}

LookupResult SkipList::Find(const Key& key) const {
    if (!kv_count_) {
        return std::nullopt;
    }
    auto cur_node = 0;
    for (size_t cur_level = level_count_limit_ - 1; ~cur_level; --cur_level) {
        while (true) {
            size_t next_node = nodes_[cur_node].next[cur_level];
            int cmp = Compare(next_node, key);
            if (cmp == 0) {
                const auto& node = nodes_[next_node];
                LookupResult value(LookupResult::value_type(0));
                if (node.value_size == 0) {
                    return value;
                }
                value->resize(node.value_size);
                kvbuffer_.Write(value->data(), node.key_offset + node.key_size, node.value_size);
                return value;
            } else if (cmp < 0) {
                break;
            }
            cur_node = next_node;
        }
    }
    return std::nullopt;
}

RangeLookupResult SkipList::FindRange(const KeyRange& range) const {
    RangeLookupResult result{};
    uint32_t cur_node = range.lower ? FindNode(*range.lower) : nodes_[0].next[0];
    if (!range.including_lower && cur_node != kNil && range.lower) {
        cur_node = nodes_[cur_node].next[0];
    }
    for (; cur_node != kNil && (!range.upper || Compare(cur_node, *range.upper) < (range.including_upper ? 1 : 0));
         cur_node = nodes_[cur_node].next[0]) {
        const Node& node = nodes_[cur_node];
        Key key(node.key_size);
        Value value(node.value_size);
        kvbuffer_.Write(key.data(), node.key_offset, key.size());
        kvbuffer_.Write(value.data(), node.key_offset + node.key_size, value.size());
        result[std::move(key)] = std::move(value);
    }
    return result;
}

void SkipList::Clear() {
    kvbuffer_.Clear();
    nodes_.clear();
    nodes_.emplace_back();
    kv_count_ = 0;
}

size_t SkipList::Size() const {
    return kv_count_;
}

size_t SkipList::GetDataSizeInBytes() const {
    return kvbuffer_.GetTotalKVSizeInBytes();
}

void SkipList::MakeIndexBlockInFd(int fd) const {
    size_t total_offset = static_cast<size_t>(kv_count_) * sizeof(Index);
    for (size_t cur_node = nodes_[0].next[0], i = 0; cur_node != kNil; cur_node = nodes_[cur_node].next[0], ++i) {
        const Node& node = nodes_[cur_node];
        index_block_buffer_[i].offset = total_offset;
        index_block_buffer_[i].key_size = node.key_size;
        index_block_buffer_[i].value_size = node.value_size;
        total_offset += node.key_size + node.value_size;
    }
    write(fd, index_block_buffer_.data(), static_cast<size_t>(kv_count_) * sizeof(Index));
}

void SkipList::MakeDataBlockInFd(int fd) const {
    for (auto cur_node = nodes_[0].next[0]; cur_node != kNil; cur_node = nodes_[cur_node].next[0]) {
        const Node& node = nodes_[cur_node];
        kvbuffer_.WriteToFd(fd, node.key_offset, node.key_offset + node.value_size);
    }
}

uint32_t SkipList::FindNode(const Key& key) const {
    if (!kv_count_) {
        return 0;
    }
    auto cur_node = 0;
    for (size_t cur_level = level_count_limit_ - 1; ~cur_level; --cur_level) {
        while (true) {
            size_t next_node = nodes_[cur_node].next[cur_level];
            int cmp = Compare(next_node, key);
            if (cmp == 0) {
                return next_node;
            } else if (cmp < 0) {
                break;
            }
            cur_node = next_node;
        }
    }
    return nodes_[cur_node].next[0];
}

int SkipList::Compare(uint32_t node_index, const Key& key) const {
    int cmp =
        node_index == kNil
            ? -1
            : kvbuffer_.Compare(key.data(), nodes_[node_index].key_offset,
                                key.size() < nodes_[node_index].key_size ? key.size() : nodes_[node_index].key_size);
    cmp = cmp != 0                                   ? cmp
          : key.size() < nodes_[node_index].key_size ? -1
          : key.size() > nodes_[node_index].key_size ? 1
                                                     : 0;
    return cmp;
}

uint8_t SkipList::RandomLevel() {
    uint8_t level = 0;
    while (level < level_count_limit_ && rng_gen_() % 2) {
        ++level;
    }
#ifndef NDEBUG
    ++statistics[level];
#endif
    return level + 1;
}

void SkipList::WriteToNode(Node& node, const Key& key, const Value& value) {
    node.key_offset = kvbuffer_.GetTotalKVSizeInBytes();
    node.key_size = key.size();
    node.value_size = value.size();
    kvbuffer_.Append(key.data(), key.size());
    kvbuffer_.Append(value.data(), value.size());
}

}  // namespace MyLSMTree::Memtable
