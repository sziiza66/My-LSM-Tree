#include "bench.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

#include "lsm_tree/common.h"
#include "lsm_tree/lsm_tree.h"

using Clock = std::chrono::high_resolution_clock;
using ns = std::chrono::nanoseconds;

using Key = MyLSMTree::Key;
using Value = MyLSMTree::Value;
using Path = MyLSMTree::Path;
using LSMTree = MyLSMTree::LSMTree;
using KeyRange = MyLSMTree::KeyRange;

namespace Bench {

std::mt19937 gen(6);

Key MakeKey(uint64_t add) {
    Key res(16);
    uint64_t rngs[2] = {gen() + add, gen()};
    std::memcpy(res.data(), &rngs[0], sizeof(rngs[0]));
    std::memcpy(res.data() + 8, &rngs[1], sizeof(rngs[1]));
    return res;
}

Value MakeValue(size_t size) {
    Value res(size);
    return res;
}

void Benchmark(size_t N, size_t range_size, const Path& path) {
    LSMTree tree(/* параметры */ 64, 10, 100000, 2 << 30, 0.05, path);

    std::vector<std::pair<Key, Value>> kvs(N);
    for (size_t i = 0; i < N; ++i)
        kvs[i] = std::make_pair(MakeKey(), MakeValue(100));
    auto start = Clock::now();

    for (size_t i = 0; i < N; ++i)
        tree.Insert(kvs[i].first, kvs[i].second);

    auto end = Clock::now();

    kvs.clear();
    kvs.shrink_to_fit();

    double seconds = std::chrono::duration_cast<ns>(end - start).count() / 1e9;

    std::cout << "Insert N=" << N << "  ops/sec=" << N / seconds << "\n";

    std::vector<Key> keys(N);
    for (size_t i = 0; i < N; ++i)
        keys[i] = MakeKey();

    start = Clock::now();

    for (size_t i = 0; i < N; ++i)
        tree.Find(keys[i]);
    ;

    end = Clock::now();

    keys.clear();
    keys.shrink_to_fit();

    seconds = std::chrono::duration_cast<ns>(end - start).count() / 1e9;

    std::cout << "Point lookup N=" << N << "  ops/sec=" << N / seconds << "\n";

    size_t queries = N / range_size;

    std::vector<KeyRange> ranges(queries);
    for (size_t i = 0; i < queries; ++i) {
        auto& r = ranges[i];
        r.lower = MakeKey();
        uint64_t base;
        std::memcpy(&base, r.lower->data() + 8, sizeof(base));
        base += range_size;
        r.upper = r.lower;
        std::memcpy(r.upper->data() + 8, &base, sizeof(base));
        r.including_lower = true;
        r.including_upper = false;
    }

    start = Clock::now();

    for (size_t i = 0; i < queries; ++i) {
        tree.FindRange(ranges[i]);
    }

    end = Clock::now();

    seconds = std::chrono::duration_cast<ns>(end - start).count() / 1e9;

    std::cout << "Short range (size=" << range_size << ") N=" << N << "  queries/sec=" << queries / seconds << "\n";
}

}  // namespace Bench
