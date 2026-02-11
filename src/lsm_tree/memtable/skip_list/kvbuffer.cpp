#include "kvbuffer.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace MyLSMTree {
namespace Memtable {

KVBuffer::KVBuffer(uint32_t slice_size) : slice_size_(slice_size) {
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
        if (slices_.back().size == slice_size_) {
            AllocateSlice();
            if (!AllocatedSuccessfully()) {
                DeleteInvalidSlicesAndThrow(slices_.size() - original_slice_count);
            }
        }

        uint32_t to_write = size < slice_size_ - slices_.back().size ? size : slice_size_ - slices_.back().size;
        std::memcpy(slices_.back().data + slices_.back().size, data, to_write);

        data += to_write;
        size -= to_write;
        slices_.back().size += to_write;
    }
}

size_t KVBuffer::GetTotalKVSizeInBytes() const {
    return (slices_.size() - 1) * slice_size_ + slices_.back().size;
}

size_t KVBuffer::GetKVBufferSliceSize() const {
    return slice_size_;
}

int KVBuffer::Compare(const uint8_t* lhs, size_t rhs_offset, uint32_t size) const {
    uint32_t i = rhs_offset / slice_size_;
    uint32_t j = (rhs_offset + size) / slice_size_;
    uint32_t rem = rhs_offset % slice_size_;
    uint32_t to_cmp = size < slice_size_ - rem ? size : slice_size_ - rem;
    int res = std::memcmp(lhs, slices_[i].data + rem, to_cmp);
    lhs += to_cmp;
    size -= to_cmp;
    ++i;
    while (i < j && res == 0) {
        res = std::memcmp(lhs, slices_[i].data, slice_size_);
        lhs += slice_size_;
        size -= slice_size_;
    }
    if (res != 0) {
        return res;
    }
    if (size) {
        res = std::memcmp(lhs, slices_[j].data, size);
    }

    return res;
}

void KVBuffer::Write(uint8_t* dest, size_t offset, uint32_t size) const {
    uint32_t i = offset / slice_size_;
    uint32_t j = (offset + size) / slice_size_;
    uint32_t rem = offset % slice_size_;
    uint32_t to_write = size < slice_size_ - rem ? size : slice_size_ - rem;
    std::memcpy(dest, slices_[i].data + rem, to_write);
    dest += to_write;
    size -= to_write;
    ++i;
    for (; i < j; ++i) {
        std::memcpy(dest, slices_[i].data, slice_size_);
        dest += slice_size_;
        size -= slice_size_;
    }
    if (size) {
        std::memcpy(dest, slices_[j].data, size);
    }
}

void KVBuffer::WriteToFd(int fd, size_t offset, uint32_t size) const {
    uint32_t i = offset / slice_size_;
    uint32_t j = (offset + size) / slice_size_;
    uint32_t rem = offset % slice_size_;
    uint32_t to_write = size < slice_size_ - rem ? size : slice_size_ - rem;
    write(fd, slices_[i].data + rem, to_write);
    size -= to_write;
    ++i;
    for (; i < j; ++i) {
        write(fd, slices_[i].data, slice_size_);
        size -= slice_size_;
    }
    if (size) {
        write(fd, slices_[j].data, size);
    }
}

void KVBuffer::Clear() {
    // for (size_t i = 1; i < slices_.size(); ++i) {
    //     std::free(slices_[i].data);
    // }
    // slices_.resize(1);
    // slices_.back().size = 0;
    for (auto& slice : slices_) {
        slice.size = 0;
    }
}

void KVBuffer::AllocateSlice() {
    slices_.emplace_back(static_cast<uint8_t*>(std::malloc(slice_size_)), 0);
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
