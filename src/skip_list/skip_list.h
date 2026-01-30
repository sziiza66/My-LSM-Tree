#pragma once

#include <cstdint>
#include <vector>

#include "kvbuffer.h"

namespace MyLSMTree {
namespace Memtable {

class SkipList {
    static constexpr uint32_t kMaxLevel = 32;

    struct Node {
        uint32_t next[kMaxLevel];

        uint32_t key_offset;
        uint32_t key_size;

        uint32_t value_offset;
        uint32_t value_size;

        uint8_t height;
    };

public:
private:
    std::vector<Node> nodes_;
    KVBuffer kvbuffer_;
};

}  // namespace Memtable
}  // namespace MyLSMTree
