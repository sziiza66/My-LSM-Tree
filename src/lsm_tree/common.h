#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace MyLSMTree {

using Offset = size_t;

struct KeyAccessToken {
    const Offset kv_offset;
};

struct ValueAccessToken {
    const Offset value_offset;
    const size_t value_size;
};

struct Index {
    size_t offset;
    uint32_t key_size;
    uint32_t value_size;
};

struct MetaBlock {
    Offset filter_offset;
    size_t filter_bits_count;
    size_t filter_hash_func_count;
    Offset index_offset;
    size_t kv_count;
};

using Key = std::vector<uint8_t>;
using KeyPtr = std::unique_ptr<Key>;
using Value = std::vector<uint8_t>;
using Values = std::vector<Value>;
using RangeLookupResult = std::map<Key, Value>;
using LookupResult = std::optional<std::vector<uint8_t>>;
using Path = std::filesystem::path;

struct KeyWithValueToken {
    Key key;
    ValueAccessToken token;
};

struct KVSizes {
    size_t key_size;
    size_t value_size;
};

struct KeyRange {
    std::optional<Key> lower;
    std::optional<Key> upper;
    bool including_lower;
    bool including_upper;
};

struct IncompleteRangeLookupResult {
    RangeLookupResult accumutaled;
    std::set<Key> deleted;
};

std::pair<uint64_t, uint64_t> CalculateHash(const uint8_t* data, size_t size);
uint64_t CalculateIthHash(uint64_t low64, uint64_t high64, size_t i, size_t mod);
uint64_t CalculateIthHash(const uint8_t* data, size_t size, size_t i, size_t mod);

}  // namespace MyLSMTree
