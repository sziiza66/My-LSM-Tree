#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>
#include "lsm_tree/memtable/memtable.h"

void Test_SkipListCorrectness() {
    using MyLSMTree::Memtable::SkipList;
    using namespace MyLSMTree::Memtable;

    struct Param {
        uint32_t key_size;
        uint32_t value_size;
    };

    size_t kvs_cnt = 100;
    size_t max_key_size = 2000;
    size_t max_value_size = 10000;

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
            auto cmp = list.Find(val.data(), kvs[j].data(), params[j].key_size);
            assert(cmp != LookUpResult::ValueNotFound);
            if (params[j].value_size == 0) {
                assert(cmp == LookUpResult::ValueDeleted);
            } else {
                assert(std::memcmp(val.data(), kvs[j].data() + params[j].key_size, params[j].value_size) == 0);
            }
        }
        for (size_t j = kvs.size() - 1; ~j; --j) {
            std::vector<uint8_t> val(params[j].value_size, 0);
            auto cmp = list.Find(val.data(), kvs[j].data(), params[j].key_size);
            assert(cmp != LookUpResult::ValueNotFound);
            if (params[j].value_size == 0) {
                assert(cmp == LookUpResult::ValueDeleted);
            } else {
                assert(std::memcmp(val.data(), kvs[j].data() + params[j].key_size, params[j].value_size) == 0);
            }
        }

        for (size_t j = 0; j < kvs.size(); j += 2) {
            list.Erase(kvs[j].data(), params[j].key_size);
        }

        for (size_t j = 0; j < kvs.size(); ++j) {
            std::vector<uint8_t> val(params[j].value_size, 0);
            auto cmp = list.Find(val.data(), kvs[j].data(), params[j].key_size);
            assert(cmp != LookUpResult::ValueNotFound);
            if (params[j].value_size == 0 || j % 2 == 0) {
                assert(cmp == LookUpResult::ValueDeleted);
            } else {
                assert(std::memcmp(val.data(), kvs[j].data() + params[j].key_size, params[j].value_size) == 0);
            }
        }

        std::vector<uint8_t> a(max_key_size + 1, 'a');
        std::vector<uint8_t> b(max_key_size + 1, 'b');
        std::vector<uint8_t> c(max_key_size + 1, 'c');
        assert(list.Find(nullptr, a.data(), a.size()) == LookUpResult::ValueNotFound);
        assert(list.Find(nullptr, b.data(), b.size()) == LookUpResult::ValueNotFound);
        assert(list.Find(nullptr, c.data(), c.size()) == LookUpResult::ValueNotFound);
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
    using MyLSMTree::Memtable::Memtable;
    using namespace MyLSMTree::Memtable;

    struct Param {
        uint32_t key_size;
        uint32_t value_size;
    };

    size_t kvs_cnt = 100;
    size_t max_key_size = 2000;
    size_t max_value_size = 10000;

    for (size_t i = 0; i < 100; ++i) {
        // std::cout << i << std::endl;
        std::mt19937 gen(i + 100);
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
        Memtable table(10000, 100000, 12, 10000, 6);
        for (size_t j = 0; j < kvs.size(); ++j) {
            table.Insert(kvs[j].data(), params[j].key_size, params[j].value_size);
        }

        for (size_t j = 0; j < kvs.size(); ++j) {
            std::vector<uint8_t> val(params[j].value_size, 0);
            auto cmp = table.Find(val.data(), kvs[j].data(), params[j].key_size);
            assert(cmp != LookUpResult::ValueNotFound);
            if (params[j].value_size == 0) {
                assert(cmp == LookUpResult::ValueDeleted);
            } else {
                assert(std::memcmp(val.data(), kvs[j].data() + params[j].key_size, params[j].value_size) == 0);
            }
        }
        for (size_t j = kvs.size() - 1; ~j; --j) {
            std::vector<uint8_t> val(params[j].value_size, 0);
            auto cmp = table.Find(val.data(), kvs[j].data(), params[j].key_size);
            assert(cmp != LookUpResult::ValueNotFound);
            if (params[j].value_size == 0) {
                assert(cmp == LookUpResult::ValueDeleted);
            } else {
                assert(std::memcmp(val.data(), kvs[j].data() + params[j].key_size, params[j].value_size) == 0);
            }
        }

        for (size_t j = 0; j < kvs.size(); j += 2) {
            table.Erase(kvs[j].data(), params[j].key_size);
        }

        for (size_t j = 0; j < kvs.size(); ++j) {
            std::vector<uint8_t> val(params[j].value_size, 0);
            auto cmp = table.Find(val.data(), kvs[j].data(), params[j].key_size);
            assert(cmp != LookUpResult::ValueNotFound);
            if (params[j].value_size == 0 || j % 2 == 0) {
                assert(cmp == LookUpResult::ValueDeleted);
            } else {
                assert(std::memcmp(val.data(), kvs[j].data() + params[j].key_size, params[j].value_size) == 0);
            }
        }

        std::vector<uint8_t> a(max_key_size + 1, 'a');
        std::vector<uint8_t> b(max_key_size + 1, 'b');
        std::vector<uint8_t> c(max_key_size + 1, 'c');
        assert(table.Find(nullptr, a.data(), a.size()) == LookUpResult::ValueNotFound);
        assert(table.Find(nullptr, b.data(), b.size()) == LookUpResult::ValueNotFound);
        assert(table.Find(nullptr, c.data(), c.size()) == LookUpResult::ValueNotFound);
    }
}
