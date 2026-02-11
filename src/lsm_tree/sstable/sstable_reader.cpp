#include <cassert>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sstable_reader.h"

namespace MyLSMTree::SSTable {

using SSTableReader = SSTableReadersManager::SSTableReader;
using KeyAccessToken = SSTableReader::KeyAccessToken;
using ValueAccessToken = SSTableReader::ValueAccessToken;

Offset KeyAccessToken::KVOffset() const {
    return kv_offset_;
}

KeyAccessToken::KeyAccessToken(Offset kv_offset) : kv_offset_(kv_offset) {
}

Offset ValueAccessToken::ValueOffset() const {
    return value_offset_;
}
size_t ValueAccessToken::ValueSize() const {
    return value_size_;
}

ValueAccessToken::ValueAccessToken(Offset value_offset, size_t value_size)
    : value_offset_(value_offset), value_size_(value_size) {
}

SSTableReader::SSTableReader(SSTableReadersManager& manager, const Path& path, int fd)
    : manager_(&manager), path_(path), fd_(fd) {
    struct stat st;
    if (fstat(fd, &st) != 0) {
        perror("fstat failed");
        return;
    }
    pread(fd_, &meta_, sizeof(meta_), st.st_size - sizeof(meta_));
}

SSTableReader::SSTableReader(SSTableReader&& other)
    : meta_(other.meta_),
      manager_(std::exchange(other.manager_, nullptr)),
      path_(std::move(other.path_)),
      fd_(std::exchange(other.fd_, -1)) {
}

SSTableReader::~SSTableReader() noexcept {
    if (manager_) {
        manager_->DecreaseFdCounter(path_);
    }
}

size_t SSTableReader::GetKVCount() const {
    return meta_.kv_count;
}

bool SSTableReader::GetFilterIthBit(size_t i) const {
    size_t batch_offset = GetFilterBatchOffsetWithIthBit(i);
    uint64_t batch;
    pread(fd_, &batch, sizeof(batch), batch_offset);
    return batch & (1ULL << (i & 63));
}

bool SSTableReader::TestHash(uint64_t hash) const {
    return GetFilterIthBit(hash % meta_.filter_bits_count);
}

bool SSTableReader::TestHashes(uint64_t low_hash, uint64_t high_hash) const {
    for (size_t i = 0; i < meta_.filter_hash_func_count; ++i) {
        if (!TestHash(low_hash + i * high_hash)) {
            return false;
        }
    }
    return true;
}

SSTableReader::KeyAccessToken SSTableReader::GetIthKeyToken(size_t i) const {
    Offset offset;
    pread(fd_, &offset, sizeof(offset), GetIthOffsetOffset(i));
    return {offset};
}

SSTableReader::KeyWithValueToken SSTableReader::GetFirstKey() const {
    KVSizes sizes;
    pread(fd_, &sizes, sizeof(sizes), 0);
    Key buffer(sizes.key_size);
    pread(fd_, buffer.data(), buffer.size() * sizeof(buffer[0]), 0 + sizeof(sizes));
    return {std::move(buffer), {0 + sizeof(sizes) + sizes.key_size, sizes.value_size}};
}

SSTableReader::KeyWithValueToken SSTableReader::GetKeyFromToken(KeyAccessToken token, Key buffer) const {
    KVSizes sizes;
    pread(fd_, &sizes, sizeof(sizes), token.kv_offset_);
    buffer.resize(sizes.key_size);
    pread(fd_, buffer.data(), buffer.size() * sizeof(buffer[0]), token.kv_offset_ + sizeof(sizes));
    return {std::move(buffer), {token.kv_offset_ + sizeof(sizes) + sizes.key_size, sizes.value_size}};
}

SSTableReader::KeyWithValueToken SSTableReader::GetNextKey(KeyWithValueToken token) const {
    KVSizes sizes;
    pread(fd_, &sizes, sizeof(sizes), token.value_token.ValueOffset() + token.value_token.ValueSize());
    token.key.resize(sizes.key_size);
    pread(fd_, token.key.data(), token.key.size() * sizeof(token.key[0]),
          token.value_token.ValueOffset() + token.value_token.ValueSize() + sizeof(sizes));
    token.value_token.value_offset_ =
        token.value_token.ValueOffset() + token.value_token.ValueSize() + sizeof(sizes) + sizes.key_size;
    token.value_token.value_size_ = sizes.value_size;
    return token;
}

Value SSTableReader::GetValueFromToken(ValueAccessToken token, Value buffer) const {
    buffer.resize(token.value_size_);
    pread(fd_, buffer.data(), buffer.size() * sizeof(buffer[0]), token.value_offset_);
    return buffer;
}

std::pair<LookupResult, Key> SSTableReader::Find(const Key& key, Key buffer) const {
    size_t l = 0;
    size_t r = meta_.kv_count + 1;
    while (l + 1 != r) {
        size_t m = (l + r) >> 1;
        auto key_token = GetIthKeyToken(m - 1);
        auto [buffer_ret, value_token] = GetKeyFromToken(key_token, std::move(buffer));
        buffer = std::move(buffer_ret);
        int cmp = Compare(key, buffer);
        if (cmp < 0) {
            r = m;
        } else if (cmp > 0) {
            l = m;
        } else {
            return {GetValueFromToken(value_token), std::move(buffer)};
        }
    }
    return {std::nullopt, std::move(buffer)};
}

std::pair<IncompleteRangeLookupResult, Key> SSTableReader::FindRange(const KeyRange& range,
                                                                     IncompleteRangeLookupResult incomplete,
                                                                     Key buffer) const {
    size_t l = 0;
    size_t r = meta_.kv_count + 1;
    if (range.lower.has_value()) {
        while (l + 1 < r) {
            size_t m = (l + r) >> 1;
            auto key_token = GetIthKeyToken(m - 1);
            auto [buffer_ret, value_token] = GetKeyFromToken(key_token, std::move(buffer));
            buffer = std::move(buffer_ret);
            int cmp = Compare(*range.lower, buffer);
            if (cmp < 0) {
                r = m;
            } else if (cmp > 0) {
                l = m;
            } else {
                l = range.including_lower ? m - 1 : m;
                break;
            }
        }
    }
    for (; l < meta_.kv_count; ++l) {
        auto key_token = GetIthKeyToken(l);
        auto [buffer_ret, value_token] = GetKeyFromToken(key_token, std::move(buffer));
        buffer = std::move(buffer_ret);
        if (range.upper.has_value() && (range.including_upper ? buffer > *range.upper : buffer >= *range.upper)) {
            break;
        }
        if (incomplete.accumutaled.find(buffer) != incomplete.accumutaled.end() ||
            incomplete.deleted.find(buffer) != incomplete.deleted.end()) {
            continue;
        }
        Value value = GetValueFromToken(value_token);
        if (value.empty()) {
            incomplete.deleted.insert(std::move(buffer));
        } else {
            incomplete.accumutaled[std::move(buffer)] = std::move(value);
        }
    }

    return {std::move(incomplete), std::move(buffer)};
}

Offset SSTableReader::GetIthOffsetOffset(size_t i) const {
    return meta_.index_offset + i * sizeof(Offset);
}

size_t SSTableReader::GetFilterBatchOffsetWithIthBit(size_t i) const {
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
        throw std::runtime_error(std::string("Can't read sstable with name ") + path.c_str() + ": " +
                                 std::strerror(errno));
    }
    fd_mapping_[normal_path] = {1, fd};
    return SSTableReader(*this, normal_path, fd);
}

size_t SSTableReadersManager::CacheSize() const {
    return cache_size_;
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
