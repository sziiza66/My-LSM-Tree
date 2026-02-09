#pragma once

#include <queue>

#include "../common.h"

namespace MyLSMTree::SSTable {

class SSTableReadersManager {
    struct FdCounter {
        uint32_t count;
        int fd;
    };

    class SSTableReader {
    public:
        SSTableReader(SSTableReadersManager* manager, const Path& path, int fd);
        SSTableReader(const SSTableReader&) = delete;
        ~SSTableReader();

        size_t GetKVCount() const;
        Index GetIthIndex(size_t i) const;
        bool GetFilterIthBit(size_t i) const;
        Key GetIthKey(size_t i) const;
        Value GetIthValue(size_t i) const;

    private:
        size_t GetIthIndexOffset(size_t i) const;
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

private:
    void DecreaseFdCounter(const Path& normal_path);
    void TryClearingCache();

private:
    std::queue<Path> cache_queue_;
    std::map<Path, FdCounter> fd_mapping_;
    size_t cache_size_;
};

}  // namespace MyLSMTree::SSTable
