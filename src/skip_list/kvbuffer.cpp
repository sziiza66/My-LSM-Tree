#include "kvbuffer.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace MyLSMTree {
namespace Memtable {

KVBuffer::KVBuffer(uint32_t max_slice_size) : max_slice_size_(max_slice_size) {
    AllocateSlice();
    if (!AllocatedSuccessfully()) {
        DeleteInvalidSlicesAndThrow(0);
    }
}

KVBuffer::~KVBuffer() noexcept {
    for (const auto& slice : slices_) {
        std::free(slice.data);
    }
}

void KVBuffer::Append(const uint8_t* data, uint32_t size) {
    uint32_t original_slice_count = slices_.size();
    while (size) {
        if (slices_.back().size == 0) {
            AllocateSlice();
            if (!AllocatedSuccessfully()) {
                DeleteInvalidSlicesAndThrow(slices_.size() - original_slice_count);
            }
        }

        uint32_t to_write = size < slices_.back().size ? size : slices_.back().size;
        std::memcpy(slices_.back().data + slices_.back().size, data, to_write);

        data += to_write;
        size -= to_write;
    }
}

bool KVBuffer::WriteAll(int fd) const {
    for (const auto& slice : slices_) {
        if (write(fd, slice.data, slice.size) != slice.size) {
            return false;
        }
    }
    return true;
}

void KVBuffer::Clear() {
    for (size_t i = 1; i < slices_.size(); ++i) {
        std::free(slices_[i].data);
    }
    slices_.resize(1);
    slices_.back().size = 0;
}

void KVBuffer::AllocateSlice() {
    slices_.emplace_back(static_cast<uint8_t*>(std::malloc(max_slice_size_)), 0);
}

bool KVBuffer::AllocatedSuccessfully() const {
    return slices_.back().data;
}

void KVBuffer::DeleteInvalidSlicesAndThrow(uint32_t slices_to_pop_count) {
    for (size_t i = slices_.size() - slices_to_pop_count; i < slices_.size(); ++i) {
        std::free(slices_[i].data);
    }
    slices_.resize(slices_.size() - slices_to_pop_count);
    throw std::runtime_error("KVBuffer can't allocate memory.");
}

}  // namespace Memtable
}  // namespace MyLSMTree
