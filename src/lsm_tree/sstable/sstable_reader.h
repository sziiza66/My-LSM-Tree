#pragma once

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

        struct KeyAccessToken {
            Offset kv_offset_;
        };

        struct ValueAccessToken {
            Offset value_offset_;
            size_t value_size_;
        };

        struct KeyWithValueToken {
            Key key;
            ValueAccessToken value_token;
        };

    public:
        class KVIterator {
            friend class SSTableReader;

        public:
            bool IsEnd() const;
            void operator++();
            const Key& GetKey() const;
            Value GetValue(Value buffer) const;
            size_t GetValueSize() const;

        private:
            KVIterator(KeyWithValueToken kv, const SSTableReader& parent);

        private:
            KeyWithValueToken kv_;
            const SSTableReader* parent_;
        };

    public:
        SSTableReader(const SSTableReader&) = delete;
        SSTableReader(SSTableReader&&);
        ~SSTableReader() noexcept;

        size_t GetKVCount() const;
        bool GetFilterIthBit(size_t i) const;
        bool TestHash(uint64_t hash) const;
        bool TestHashes(uint64_t low_hash, uint64_t high_hash) const;
        std::pair<LookupResult, Key> Find(const Key& key, Key buffer = {}) const;
        std::pair<RangeLookupResult, Key> FindRange(const KeyRange& range,
                                                              RangeLookupResult accumulated = {},
                                                              Key buffer = {}) const;
        KVIterator Begin() const;

    private:
        SSTableReader(SSTableReadersManager& manager, const Path& path, int fd);

        Offset GetIthOffsetOffset(size_t i) const;
        size_t GetFilterBatchOffsetWithIthBit(size_t i) const;
        KeyAccessToken GetIthKeyToken(size_t i) const;
        KeyWithValueToken GetFirstKey() const;
        KeyWithValueToken GetKeyFromToken(KeyAccessToken token, Key buffer = {}) const;
        KeyWithValueToken GetNextKey(KeyWithValueToken token) const;
        Value GetValueFromToken(ValueAccessToken token, Value buffer = {}) const;

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
    void Unlink(const Path& path);

private:
    void DecreaseFdCounter(const Path& normal_path);
    void TryClearingCache();

private:
    std::queue<Path> cache_queue_;
    std::map<Path, FdCounter> fd_mapping_;
    size_t cache_size_;
};

}  // namespace MyLSMTree::SSTable
