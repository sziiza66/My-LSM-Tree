#pragma once

#include "lsm_tree/common.h"

namespace Bench {

MyLSMTree::Key MakeKey(uint64_t add = 0);
MyLSMTree::Value MakeValue();
void Benchmark(size_t N, size_t range_size, const MyLSMTree::Path& path);

}  // namespace Bench
