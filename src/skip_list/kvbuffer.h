#pragma once

#include <cstdint>
#include <unistd.h>
#include <vector>

namespace MyLSMTree {
namespace Memtable {

class KVBuffer {
    struct Slice {
        uint8_t* data;
        uint32_t size;
    };

public:
    KVBuffer(uint32_t max_slice_size);
    ~KVBuffer() noexcept;

    void Append(const uint8_t* data, uint32_t size);
    bool WriteAll(int fd) const;

    void Clear();

private:
    void AllocateSlice();
    bool AllocatedSuccessfully() const;
    void DeleteInvalidSlicesAndThrow(uint32_t slices_to_pop_count);

private:
    std::vector<Slice> slices_;
    uint32_t max_slice_size_;
};

}  // namespace Memtable
}  // namespace MyLSMTree
