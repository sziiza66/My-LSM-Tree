#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MyLSMTree::ReverseIndex {

using Token = std::string;
using TokenId = uint64_t;
using DocId = uint64_t;
using NGram = std::vector<uint8_t>;

}  // namespace MyLSMTree::ReverseIndex
