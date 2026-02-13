#ifndef NDEBUG
#include "tests.h"
#else
#include <iostream>

#include "bench.h"
#endif

int main() {
#ifndef NDEBUG
    Test::Test_All();
#else
    std::vector<size_t> sizes = {
        100'000,
        1'000'000,
        5'000'000,
        5'000'000'0,
    };

    for (auto N : sizes) {
        Bench::Benchmark(N, 10, "tree_data.data");
        std::cout << "--------------------------\n";
    }
#endif

    return 0;
}
