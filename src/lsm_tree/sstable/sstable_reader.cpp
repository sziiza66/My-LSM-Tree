#include <cassert>
#include <stdexcept>
#include <utility>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sstable_reader.h"

namespace MyLSMTree::SSTable {

SSTableReadersManager::SSTableReader::SSTableReader(SSTableReadersManager& manager, const Path& path, int fd)
    : manager_(&manager), path_(path), fd_(fd) {
    struct stat st;
    if (fstat(fd, &st) != 0) {
        perror("fstat failed");
        return;
    }
    pread(fd_, &meta_, sizeof(meta_), st.st_size - sizeof(meta_));
}

SSTableReadersManager::SSTableReader::SSTableReader(SSTableReader&& other)
    : manager_(std::exchange(other.manager_, nullptr)),
      path_(std::move(other.path_)),
      fd_(std::exchange(other.fd_, -1)) {
}

SSTableReadersManager::SSTableReader::~SSTableReader() {
    manager_->DecreaseFdCounter(path_);
}

size_t SSTableReadersManager::SSTableReader::GetKVCount() const {
    return meta_.kv_count;
}

bool SSTableReadersManager::SSTableReader::GetFilterIthBit(size_t i) const {
    size_t batch_offset = GetFilterBatchOffsetWithIthBit(i);
    uint64_t batch;
    pread(fd_, &batch, sizeof(batch), batch_offset);
    return batch & (1ULL << (i & 63));
}

bool SSTableReadersManager::SSTableReader::TestHash(uint64_t hash) const {
    return GetFilterIthBit(hash % meta_.filter_bits_count);
}

bool SSTableReadersManager::SSTableReader::TestHashes(uint64_t low_hash, uint64_t high_hash) const {
    for (size_t i = 0; i < meta_.filter_hash_func_count; ++i) {
        if (!TestHash(low_hash + i * high_hash)) {
            return false;
        }
    }
    return true;
}

Offset SSTableReadersManager::SSTableReader::GetIthOffset(size_t i) const {
    Offset offset;
    pread(fd_, &offset, sizeof(offset), GetIthOffsetOffset(i));
    return offset;
}

// Key SSTableReadersManager::SSTableReader::GetIthKey(size_t i) const {
//     Index index = GetIthIndex(i);
//     Key key(index.key_size);
//     pread(fd_, key.data(), key.size(), index.offset);
//     return key;
// }

// Value SSTableReadersManager::SSTableReader::GetIthValue(size_t i) const {
//     Index index = GetIthIndex(i);
//     Value value(index.value_size);
//     pread(fd_, value.data(), value.size(), index.offset + index.key_size);
//     return value;
// }

std::pair<LookupResult, Key> SSTableReadersManager::SSTableReader::Find(const Key& key, Key buffer) const {
    size_t l = 0;
    size_t r = meta_.kv_count + 1;
    while (l + 1 != r) {
        size_t m = (l + r) >> 1;
        Offset offset = GetIthOffset(m - 1);
        auto [buffer_ret, value_offset] = GetKeyFromOffset(offset, std::move(buffer));
        buffer = std::move(buffer_ret);
        if (key < buffer) {
            r = m;
        } else if (key > buffer) {
            l = m;
        } else {
            return {GetValueFromOffset(value_offset), std::move(buffer)};
        }
    }
    return {std::nullopt, std::move(buffer)};
}

std::pair<IncompleteRangeLookupResult, Key> SSTableReadersManager::SSTableReader::FindRange(
    const KeyRange& range, IncompleteRangeLookupResult incomplete, Key buffer) const {
    size_t l = 0;
    size_t r = meta_.kv_count;
    if (range.lower) {
        while (l + 1 < r) {
            size_t m = (l + r) >> 1;
        Offset offset = GetIthOffset(m - 1);
        auto [buffer_ret, value_offset] = GetKeyFromOffset(offset, std::move(buffer));
        buffer = std::move(buffer_ret);
            if (range.lower < buffer) {
                r = m;
            } else if (range.lower > buffer) {
                l = m;
            } else {
                l = range.including_lower ? m : m + 1;
                break;
            }
        }
    }

    for (; l < meta_.kv_count; ++l) {
        Offset offset = GetIthOffset(l);
        auto [buffer_ret, value_offset] = GetKeyFromOffset(offset, std::move(buffer));
        buffer = std::move(buffer_ret);
        if (range.upper && (range.including_upper ? buffer > range.upper : buffer >= range.upper)) {
            break;
        }
        if (incomplete.accumutaled.find(buffer) != incomplete.accumutaled.end() ||
            incomplete.deleted.find(buffer) != incomplete.deleted.end()) {
            continue;
        }
        size_t value_size;
        pread(fd_, &value_size, sizeof(value_size), value_offset);
        if (!value_size) {
            incomplete.deleted.insert(std::move(buffer));
        } else {
            Value value = GetValueFromOffset(value_offset);
            incomplete.accumutaled[std::move(buffer)] = std::move(value);
        }
    }

    return {std::move(incomplete), std::move(buffer)};
}

KeyWithValueOffset SSTableReadersManager::SSTableReader::GetKeyFromOffset(Offset offset, Key buffer) const {
    size_t key_size;
    pread(fd_, &key_size, sizeof(key_size), offset);
    buffer.resize(key_size);
    pread(fd_, buffer.data(), buffer.size(), offset + sizeof(key_size));
    return {std::move(buffer), offset + sizeof(key_size) + key_size};
}

Value SSTableReadersManager::SSTableReader::GetValueFromOffset(Offset offset) const {
    size_t value_size;
    pread(fd_, &value_size, sizeof(value_size), offset);
    Value value(value_size);
    pread(fd_, value.data(), value.size(), offset + value_size);
    return value;
}

Offset SSTableReadersManager::SSTableReader::GetIthOffsetOffset(size_t i) const {
    return meta_.index_offset + i * sizeof(Offset);
}

size_t SSTableReadersManager::SSTableReader::GetFilterBatchOffsetWithIthBit(size_t i) const {
    return meta_.filter_offset + (i / 64) * 8;
}

SSTableReadersManager::SSTableReadersManager(size_t cahce_size) : cache_size_(cahce_size) {
}

SSTableReadersManager::SSTableReader SSTableReadersManager::CreateReader(const Path& path) {
    auto normal_path = path.lexically_normal();
    if (auto it = fd_mapping_.find(normal_path); it != fd_mapping_.end()) {
        ++it->second.count;
        return SSTableReader(*this, normal_path, it->second.fd);
    }
    int fd = open(normal_path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        throw std::runtime_error(std::string("There is no sstable with name ") + path.c_str());
    }
    fd_mapping_[normal_path] = {1, fd};
    return SSTableReader(*this, normal_path, fd);
}

void SSTableReadersManager::DecreaseFdCounter(const Path& normal_path) {
    auto it = fd_mapping_.find(normal_path);
    assert(it != fd_mapping_.end());
    --it->second.count;
    if (!it->second.count) {
        cache_queue_.push(normal_path);
        TryClearingCache();
    }
}

void SSTableReadersManager::TryClearingCache() {
    while (cache_queue_.size() > cache_size_) {
        Path normal_path = std::move(cache_queue_.front());
        cache_queue_.pop();
        auto it = fd_mapping_.find(normal_path);
        if (it == fd_mapping_.end()) {
            continue;
        }
        if (!it->second.count) {
            close(it->second.fd);
            fd_mapping_.erase(it);
        }
    }
}

}  // namespace MyLSMTree::SSTable
