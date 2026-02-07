#pragma once

#include <cstdint>
#include <unistd.h>
#include <vector>

namespace MyLSMTree::Memtable {

class KVBuffer {
    struct Slice {
        uint8_t* data;
        uint32_t size;
    };

public:
    KVBuffer(uint32_t slice_size);
    ~KVBuffer() noexcept;

    void Append(const uint8_t* data, uint32_t size);
    size_t GetTotalKVSizeInBytes() const;
    int Compare(const uint8_t* lhs, size_t rhs_offset, uint32_t size) const;
    void Write(uint8_t* dest, size_t offset, uint32_t size) const;
    // bool WriteAllToFd(int fd) const;

    void Clear();

private:
    void AllocateSlice();
    bool AllocatedSuccessfully() const;
    void DeleteInvalidSlicesAndThrow(uint32_t slices_to_pop_count);

private:
    std::vector<Slice> slices_;
    uint32_t slice_size_;
};

}  // namespace MyLSMTree::Memtable
