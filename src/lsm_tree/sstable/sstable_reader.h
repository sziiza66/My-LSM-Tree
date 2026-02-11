#pragma once

#include <memory>
#include <queue>

#include "../common.h"

namespace MyLSMTree::SSTable {

class SSTableReadersManager {
    struct FdCounter {
        uint32_t count;
        int fd;
    };

public:
    class SSTableReader {
        friend class SSTableReadersManager;
    public:
        class KeyAccessToken {
            friend class SSTableReadersManager::SSTableReader;

        public:
            Offset KVOffset() const;

        private:
            KeyAccessToken(Offset kv_offset);

        private:
            Offset kv_offset_;
        };

        class ValueAccessToken {
            friend class SSTableReadersManager::SSTableReader;

        public:
            Offset ValueOffset() const;
            size_t ValueSize() const;

        private:
            ValueAccessToken(Offset value_offset, size_t value_size);

        private:
            Offset value_offset_;
            size_t value_size_;
        };

        struct KeyWithValueToken {
            Key key;
            ValueAccessToken value_token;
        };

    public:
        SSTableReader(const SSTableReader&) = delete;
        SSTableReader(SSTableReader&&);
        ~SSTableReader() noexcept;

        size_t GetKVCount() const;
        bool GetFilterIthBit(size_t i) const;
        bool TestHash(uint64_t hash) const;
        bool TestHashes(uint64_t low_hash, uint64_t high_hash) const;
        KeyAccessToken GetIthKeyToken(size_t i) const;
        KeyWithValueToken GetFirstKey() const;
        KeyWithValueToken GetKeyFromToken(KeyAccessToken token, Key buffer = {}) const;
        KeyWithValueToken GetNextKey(KeyWithValueToken token) const;
        Value GetValueFromToken(ValueAccessToken token, Value buffer = {}) const;
        std::pair<LookupResult, Key> Find(const Key& key, Key buffer = {}) const;
        std::pair<IncompleteRangeLookupResult, Key> FindRange(const KeyRange& range,
                                                              IncompleteRangeLookupResult incomplete = {},
                                                              Key buffer = {}) const;

    private:
        SSTableReader(SSTableReadersManager& manager, const Path& path, int fd);

        Offset GetIthOffsetOffset(size_t i) const;
        size_t GetFilterBatchOffsetWithIthBit(size_t i) const;

    private:
        MetaBlock meta_;
        SSTableReadersManager* manager_;
        Path path_;
        int fd_;
    };

public:
    SSTableReadersManager(size_t cache_size);

    SSTableReader CreateReader(const Path& path);
    size_t CacheSize() const;

private:
    void DecreaseFdCounter(const Path& normal_path);
    void TryClearingCache();

private:
    std::queue<Path> cache_queue_;
    std::map<Path, FdCounter> fd_mapping_;
    size_t cache_size_;
};

}  // namespace MyLSMTree::SSTable
