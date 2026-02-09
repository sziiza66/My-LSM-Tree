#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>
#include "lsm_tree/memtable/memtable.h"
#include "lsm_tree/memtable/skip_list/skip_list.h"
#include "lsm_tree/common.h"

void Test_SkipListCorrectness() {
    using MyLSMTree::Memtable::SkipList;
    using namespace MyLSMTree::Memtable;
    using namespace MyLSMTree;

    size_t kvs_cnt = 100;
    size_t max_key_size = 2000;
    size_t max_value_size = 10000;

    for (size_t i = 0; i < 100; ++i) {
        // std::cout << i << std::endl;
        std::mt19937 gen(i);
        std::vector<Key> keys(kvs_cnt);
        std::vector<Value> vals(kvs_cnt);
        for (size_t j = 0; j < kvs_cnt; ++j) {
            bool ok;
            do {
                keys.emplace_back((uint32_t)(gen() % max_key_size + 1));
                vals.emplace_back((uint32_t)(gen() % (max_value_size + 1)));
                for (size_t k = 0; k < keys[j].size(); ++k) {
                    keys[j][k] = gen() % 256;
                }
                for (size_t k = 0; k < vals[j].size(); ++k) {
                    vals[j][k] = gen() % 256;
                }
                ok = true;
                for (size_t k = 0; k < j && ok; ++k) {
                    ok = keys[k] == keys[j];
                }
            } while (!ok);
        }
        SkipList list(10000, 10000, 6);
        for (size_t j = 0; j < kvs_cnt; ++j) {
            list.Insert(keys[j], vals[j]);
        }

        for (size_t j = 0; j < kvs_cnt; ++j) {
            LookupResult res = list.Find(keys[j]);
            assert(res);
            if (vals[j].size() == 0) {
                assert(res->empty());
            } else {
                assert(*res == vals[j]);
            }
        }
        for (size_t j = kvs_cnt - 1; ~j; --j) {
            LookupResult res = list.Find(keys[j]);
            assert(res);
            if (vals[j].size() == 0) {
                assert(res->empty());
            } else {
                assert(*res == vals[j]);
            }
        }

        for (size_t j = 0; j < kvs_cnt; j += 2) {
            list.Erase(keys[j]);
        }

        for (size_t j = 0; j < kvs_cnt; ++j) {
            LookupResult res = list.Find(keys[j]);
            assert(res);
            if (vals[j].size() == 0 || j % 2 == 0) {
                assert(res->empty());
            } else {
                assert(*res == vals[j]);
            }
        }

        Key a(max_key_size + 1, 'a');
        Key b(max_key_size + 1, 'b');
        Key c(max_key_size + 1, 'c');
        assert(!list.Find(a));
        assert(!list.Find(b));
        assert(!list.Find(c));
    }
}

void Test_FilterCorrectness() {
    using MyLSMTree::Memtable::BloomFilter;
    using namespace MyLSMTree::Memtable;

    size_t data_cnt = 100;
    size_t max_data_size = 2000;

    for (size_t i = 0; i < 100; ++i) {
        // std::cout << i << std::endl;
        std::mt19937 gen(i);
        std::vector<std::vector<uint8_t>> data(data_cnt);
        for (size_t j = 0; j < data.size(); ++j) {
            uint32_t data_size = (uint32_t)(gen() % max_data_size + 1);
            data[j].resize(data_size);
            for (size_t k = 0; k < data_size; ++k) {
                data[j][k] = gen() % 256;
            }
        }

        BloomFilter filter(3000, 6);
        for (size_t j = 0; j < data.size(); ++j) {
            filter.Insert(data[j].data(), data[j].size());
        }

        for (size_t j = 0; j < data.size(); ++j) {
            assert(filter.Find(data[j].data(), data[j].size()));
        }
    }
}

void Test_MemtableCorrectness() {
    using namespace MyLSMTree;

    size_t kvs_cnt = 100;
    size_t max_key_size = 2000;
    size_t max_value_size = 10000;

    for (size_t i = 0; i < 100; ++i) {
        // std::cout << i << std::endl;
        std::mt19937 gen(i);
        std::vector<Key> keys(kvs_cnt);
        std::vector<Value> vals(kvs_cnt);
        for (size_t j = 0; j < kvs_cnt; ++j) {
            bool ok;
            do {
                keys.emplace_back((uint32_t)(gen() % max_key_size + 1));
                vals.emplace_back((uint32_t)(gen() % (max_value_size + 1)));
                for (size_t k = 0; k < keys[j].size(); ++k) {
                    keys[j][k] = gen() % 256;
                }
                for (size_t k = 0; k < vals[j].size(); ++k) {
                    vals[j][k] = gen() % 256;
                }
                ok = true;
                for (size_t k = 0; k < j && ok; ++k) {
                    ok = keys[k] == keys[j];
                }
            } while (!ok);
        }
        Memtable::Memtable table(10000, 100000, 12, 10000, 6);
        for (size_t j = 0; j < kvs_cnt; ++j) {
            table.Insert(keys[j], vals[j]);
        }

        for (size_t j = 0; j < kvs_cnt; ++j) {
            LookupResult res = table.Find(keys[j]);
            assert(res);
            if (vals[j].size() == 0) {
                assert(res->empty());
            } else {
                assert(*res == vals[j]);
            }
        }
        for (size_t j = kvs_cnt - 1; ~j; --j) {
            LookupResult res = table.Find(keys[j]);
            assert(res);
            if (vals[j].size() == 0) {
                assert(res->empty());
            } else {
                assert(*res == vals[j]);
            }
        }

        for (size_t j = 0; j < kvs_cnt; j += 2) {
            table.Erase(keys[j]);
        }

        for (size_t j = 0; j < kvs_cnt; ++j) {
            LookupResult res = table.Find(keys[j]);
            assert(res);
            if (vals[j].size() == 0 || j % 2 == 0) {
                assert(res->empty());
            } else {
                assert(*res == vals[j]);
            }
        }

        Key a(max_key_size + 1, 'a');
        Key b(max_key_size + 1, 'b');
        Key c(max_key_size + 1, 'c');
        assert(!table.Find(a));
        assert(!table.Find(b));
        assert(!table.Find(c));
    }
}

void Test_All() {
    Test_SkipListCorrectness();
    Test_FilterCorrectness();
    Test_MemtableCorrectness();
}
