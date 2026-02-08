#pragma once

#include <cstddef>
#include <cstdint>

namespace MyLSMTree {

namespace Memtable {

enum class LookUpResult : int {
    ValueNotFound,
    ValueFound,
    ValueDeleted,
};

}  // namespace Memtable

struct Index {
    size_t offset;
    uint32_t key_size;
    uint32_t value_size;
};

}  // namespace MyLSMTree
