#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>
#include "memtable/skip_list/skip_list.h"

void Test_SkipList_Correctness() {
    struct Param {
        uint32_t key_size;
        uint32_t value_size;
    };

    size_t kvs_cnt = 100;
    size_t max_key_size = 2000;
    size_t max_value_size = 10000;

    using MyLSMTree::Memtable::SkipList;
    for (size_t i = 0; i < 100; ++i) {
        // std::cout << i << std::endl;
        std::mt19937 gen(i);
        std::vector<std::vector<uint8_t>> kvs(kvs_cnt);
        std::vector<Param> params(kvs_cnt);
        for (size_t j = 0; j < kvs.size(); ++j) {
            bool ok;
            do {
                params[j] = {(uint32_t)(gen() % max_key_size + 1), (uint32_t)(gen() % (max_value_size + 1))};
                kvs[j].resize(params[j].key_size + params[j].value_size);
                for (size_t k = 0; k < params[j].key_size + params[j].value_size; ++k) {
                    kvs[j][k] = gen() % 256;
                }
                ok = true;
                for (size_t k = 0; k < j && ok; ++k) {
                    if (params[j].key_size != params[k].key_size) {
                        continue;
                    }
                    ok = std::memcmp(kvs[j].data(), kvs[k].data(), params[j].key_size) != 0;
                }
            } while (!ok);
        }
        SkipList list(10000, 10000, 6);
        for (size_t j = 0; j < kvs.size(); ++j) {
            list.Insert(kvs[j].data(), params[j].key_size, params[j].value_size);
        }

        for (size_t j = 0; j < kvs.size(); ++j) {
            std::vector<uint8_t> val(params[j].value_size, 0);
            int cmp = list.Find(val.data(), kvs[j].data(), params[j].key_size);
            assert(cmp != 0);
            if (params[j].value_size == 0) {
                assert(cmp == -1);
            } else {
                assert(std::memcmp(val.data(), kvs[j].data() + params[j].key_size, params[j].value_size) == 0);
            }
        }
        for (size_t j = kvs.size() - 1; ~j; --j) {
            std::vector<uint8_t> val(params[j].value_size, 0);
            int cmp = list.Find(val.data(), kvs[j].data(), params[j].key_size);
            assert(cmp != 0);
            if (params[j].value_size == 0) {
                assert(cmp == -1);
            } else {
                assert(std::memcmp(val.data(), kvs[j].data() + params[j].key_size, params[j].value_size) == 0);
            }
        }

        for (size_t j = 0; j < kvs.size(); j += 2) {
            list.Erase(kvs[j].data(), params[j].key_size);
        }

        for (size_t j = 0; j < kvs.size(); ++j) {
            std::vector<uint8_t> val(params[j].value_size, 0);
            int cmp = list.Find(val.data(), kvs[j].data(), params[j].key_size);
            assert(cmp != 0);
            if (params[j].value_size == 0 || j % 2 == 0) {
                assert(cmp == -1);
            } else {
                assert(std::memcmp(val.data(), kvs[j].data() + params[j].key_size, params[j].value_size) == 0);
            }
        }

        uint8_t temp[10000];
        std::vector<uint8_t> a(1000, 'a');
        std::vector<uint8_t> b(1000, 'b');
        std::vector<uint8_t> c(1000, 'c');
        assert(list.Find(temp, a.data(), a.size()) == 0);
        assert(list.Find(temp, b.data(), b.size()) == 0);
        assert(list.Find(temp, c.data(), c.size()) == 0);
    }
}
