#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace MyLSMTree::Memtable {

class Bitset {
public:
    explicit Bitset(size_t bits_count);
    explicit Bitset(std::vector<uint64_t> data);

    bool Test(size_t i) const;
    void Set(size_t i);
    void Reset(size_t i);
    void Clear();

    const uint64_t* Data() const;
    size_t GetSizeInBytes() const;

private:
    std::vector<uint64_t> data_;
};

}  // namespace MyLSMTree::Memtable
