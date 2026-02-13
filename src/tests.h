#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <random>
#include <vector>
#include "lsm_tree/lsm_tree.h"
#include "lsm_tree/memtable/memtable.h"
#include "lsm_tree/common.h"

namespace Test {

namespace {
using namespace MyLSMTree;

Key GenerateRandomKey(std::mt19937& gen, size_t max_key_size) {
    Key res((uint32_t)(gen() % max_key_size + 1));
    for (size_t k = 0; k < res.size(); ++k) {
        res[k] = gen() % 256;
    }
    return res;
}

Value GenerateRandomValue(std::mt19937& gen, size_t max_value_size, bool may_be_empty = false) {
    size_t value_size = may_be_empty ? (gen() % (max_value_size + 1)) : (gen() % max_value_size + 1);
    Value res(value_size);
    for (size_t k = 0; k < res.size(); ++k) {
        res[k] = gen() % 256;
    }
    return res;
}

bool IsInRange(const KeyRange& range, const Key& key) {
    return (!range.lower.has_value() || (range.including_lower ? key >= *range.lower : key > *range.lower)) &&
           (!range.upper.has_value() || (range.including_upper ? key <= *range.upper : key < *range.upper));
}

}  // namespace

const std::vector<void (*)()> tests = {
    [] /*Test_BloomFilter_Correctness*/ () {
        using MyLSMTree::Memtable::BloomFilter;
        using namespace MyLSMTree::Memtable;

        size_t data_cnt = 100;
        size_t max_data_size = 2000;

        for (size_t i = 0; i < 100; ++i) {
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
                filter.Insert(data[j]);
            }

            for (size_t j = 0; j < data.size(); ++j) {
                assert(filter.Find(data[j]));
            }
            std::cout << "Test_BloomFilter_Correctness " << i << " OK" << std::endl;
        }
    },
    [] /*Test_Memtable_Correctness*/ () {
        using namespace MyLSMTree;

        size_t kvs_cnt = 100;
        size_t max_key_size = 2000;
        size_t max_value_size = 10000;

        for (size_t i = 0; i < 100; ++i) {
            std::mt19937 gen(i);
            std::vector<Key> keys(kvs_cnt);
            std::vector<Value> vals(kvs_cnt);
            for (size_t j = 0; j < kvs_cnt; ++j) {
                bool duplicate;
                do {
                    keys[j] = GenerateRandomKey(gen, max_key_size);
                    vals[j] = GenerateRandomValue(gen, max_value_size, true);
                    for (size_t k = 0; k < keys[j].size(); ++k) {
                        keys[j][k] = gen() % 256;
                    }
                    for (size_t k = 0; k < vals[j].size(); ++k) {
                        vals[j][k] = gen() % 256;
                    }
                    duplicate = false;
                    for (size_t k = 0; (k < j) && !duplicate; ++k) {
                        duplicate = keys[k] == keys[j];
                    }
                } while (duplicate);
            }
            size_t filter_bits_count = 10000;
            size_t filter_hash_func_count = 10;
            size_t kv_count_limit = 100000;
            uint32_t kv_buffer_slice_size = 10000;
            std::mt19937::result_type list_rng_seed = 6;
            Memtable::Memtable table(filter_bits_count, filter_hash_func_count, kv_count_limit, kv_buffer_slice_size,
                                     list_rng_seed);
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
            std::cout << "Test_Memtable_Correctness " << i << " OK" << std::endl;
        }
    },
    [] /*Test_Memtable_RangeSearch_Correctness*/ () {
        using namespace MyLSMTree;
        struct KV {
            Key key;
            Value value;
        };
        using KVs = std::vector<KV>;
        for (size_t i = 0; i < 100; ++i) {

            size_t kvs_cnt = 100;
            size_t max_key_size = 2000;
            size_t max_value_size = 10000;

            std::mt19937 gen(i);
            KVs kvs(kvs_cnt);
            for (size_t j = 0; j < kvs_cnt; ++j) {
                bool duplicate;
                do {
                    kvs[j] = {GenerateRandomKey(gen, max_key_size), GenerateRandomValue(gen, max_value_size, true)};
                    duplicate = false;
                    for (size_t k = 0; k < j && !duplicate; ++k) {
                        duplicate = kvs[k].key == kvs[j].key;
                    }
                } while (duplicate);
            }
            size_t filter_bits_count = 10000;
            size_t filter_hash_func_count = 10;
            size_t kv_count_limit = 100000;
            uint32_t kv_buffer_slice_size = 10000;
            std::mt19937::result_type list_rng_seed = 6;
            Memtable::Memtable table(filter_bits_count, filter_hash_func_count, kv_count_limit, kv_buffer_slice_size,
                                     list_rng_seed);
            for (size_t j = 0; j < kvs_cnt; ++j) {
                table.Insert(kvs[j].key, kvs[j].value);
            }

            std::sort(kvs.begin(), kvs.end(), [](const auto& a, const auto& b) { return a.key < b.key; });

            for (size_t p = 0; p < 16; ++p) {
                KeyRange range{.lower = std::nullopt,
                               .upper = std::nullopt,
                               .including_lower = (p & 1) != 0,
                               .including_upper = (p & 2) != 0};
                if (p & 4) {
                    range.lower = GenerateRandomKey(gen, max_key_size * 2);
                }
                if (p & 8) {
                    range.upper = GenerateRandomKey(gen, max_key_size * 2);
                }
                RangeLookupResult correct_answer;
                for (const auto& kv : kvs) {
                    if (IsInRange(range, kv.key)) {
                        if (kv.value.empty()) {
                            correct_answer.erase(kv.key);
                        } else {
                            correct_answer[kv.key] = kv.value;
                        }
                    }
                }

                RangeLookupResult table_answer = table.FindRange(range);
                assert(table_answer == correct_answer);
                assert(table_answer == correct_answer);
            }
            std::cout << "Test_Memtable_RangeSearch_Correctness " << i << " OK" << std::endl;
        }
    },
    [] /*Test_LSMTree_Correctnes_1*/ () {
        using namespace MyLSMTree;

        size_t kvs_cnt = 25600;
        size_t max_key_size = 4;
        size_t max_value_size = 20;

        for (size_t i = 0; i < 100; ++i) {
            std::mt19937 gen(i);
            std::vector<Key> keys(kvs_cnt);
            std::vector<Value> vals(kvs_cnt);
            for (size_t j = 0; j < kvs_cnt; ++j) {
                bool duplicate;
                do {
                    keys[j] = GenerateRandomKey(gen, max_key_size);
                    vals[j] = GenerateRandomValue(gen, max_value_size, true);
                    for (size_t k = 0; k < keys[j].size(); ++k) {
                        keys[j][k] = gen() % 256;
                    }
                    for (size_t k = 0; k < vals[j].size(); ++k) {
                        vals[j][k] = gen() % 256;
                    }
                    duplicate = false;
                    for (size_t k = 0; (k < j) && !duplicate; ++k) {
                        duplicate = keys[k] == keys[j];
                    }
                } while (duplicate);
            }
            size_t fd_cache_size = 10;
            size_t sstable_scaling_factor = 4;
            size_t memtable_kv_count_limit = 100;
            size_t kv_buffer_slice_size = 1000;
            double filter_false_positive_rate = 0.1;
            Path tree_data = "tree_data.data";
            MyLSMTree::LSMTree tree(fd_cache_size, sstable_scaling_factor, memtable_kv_count_limit,
                                    kv_buffer_slice_size, filter_false_positive_rate, tree_data);
            for (size_t j = 0; j < kvs_cnt; ++j) {
                if (vals[j].size() == 0) {
                    tree.Erase(keys[j]);
                } else {
                    tree.Insert(keys[j], vals[j]);
                }
            }
            for (size_t j = 0; j < kvs_cnt; ++j) {
                LookupResult res = tree.Find(keys[j]);
                if (vals[j].size() == 0) {
                    assert(!res.has_value());
                } else {
                    assert(*res == vals[j]);
                }
            }
            for (size_t j = kvs_cnt - 1; ~j; --j) {
                LookupResult res = tree.Find(keys[j]);
                if (vals[j].size() == 0) {
                    assert(!res.has_value());
                } else {
                    assert(*res == vals[j]);
                }
            }

            for (size_t j = 0; j < kvs_cnt; j += 2) {
                tree.Erase(keys[j]);
            }

            for (size_t j = 0; j < kvs_cnt; ++j) {
                LookupResult res = tree.Find(keys[j]);
                if (vals[j].size() == 0 || j % 2 == 0) {
                    assert(!res.has_value());
                } else {
                    assert(*res == vals[j]);
                }
            }

            Key a(max_key_size + 1, 'a');
            Key b(max_key_size + 1, 'b');
            Key c(max_key_size + 1, 'c');
            assert(!tree.Find(a));
            assert(!tree.Find(b));
            assert(!tree.Find(c));
            std::cout << "Test_LSMTree_Correctnes_1 " << i << " OK" << std::endl;
        }
    },
    [] /*Test_LSMTree_RangeSearch_Correctness*/ () {
        using namespace MyLSMTree;
        struct KV {
            Key key;
            Value value;
        };
        using KVs = std::vector<KV>;
        for (size_t i = 0; i < 100; ++i) {

            size_t kvs_cnt = 6400;
            size_t max_key_size = 3;
            size_t max_value_size = 20;

            std::mt19937 gen(i + 100);
            KVs kvs(kvs_cnt);
            for (size_t j = 0; j < kvs_cnt; ++j) {
                bool duplicate;
                do {
                    kvs[j] = {GenerateRandomKey(gen, max_key_size), GenerateRandomValue(gen, max_value_size, true)};
                    duplicate = false;
                    for (size_t k = 0; k < j && !duplicate; ++k) {
                        duplicate = kvs[k].key == kvs[j].key;
                    }
                } while (duplicate);
            }
            size_t fd_cache_size = 10;
            size_t sstable_scaling_factor = 5;
            size_t memtable_kv_count_limit = 100;
            size_t kv_buffer_slice_size = 1000;
            double filter_false_positive_rate = 0.1;
            Path tree_data = "tree_data.data";
            MyLSMTree::LSMTree tree(fd_cache_size, sstable_scaling_factor, memtable_kv_count_limit,
                                    kv_buffer_slice_size, filter_false_positive_rate, tree_data);
            for (size_t j = 0; j < kvs_cnt; ++j) {
                tree.Insert(kvs[j].key, kvs[j].value);
            }

            std::sort(kvs.begin(), kvs.end(), [](const auto& a, const auto& b) { return a.key < b.key; });

            for (size_t p = 0; p < 16; ++p) {
                KeyRange range{.lower = std::nullopt,
                               .upper = std::nullopt,
                               .including_lower = (p & 1) != 0,
                               .including_upper = (p & 2) != 0};
                if (p & 4) {
                    range.lower = GenerateRandomKey(gen, max_key_size * 2);
                }
                if (p & 8) {
                    range.upper = GenerateRandomKey(gen, max_key_size * 2);
                }
                RangeLookupResult correct_answer;
                for (const auto& kv : kvs) {
                    if (IsInRange(range, kv.key)) {
                        if (kv.value.empty()) {
                            continue;
                        } else {
                            correct_answer[kv.key] = kv.value;
                        }
                    }
                }

                RangeLookupResult tree_answer = tree.FindRange(range);
                assert(tree_answer == correct_answer);
            }
            std::cout << "Test_LSMTree_RangeSearch_Correctness " << i << " OK" << std::endl;
        }
    },
    [] /*Test_LSMTree_Correctnes_2*/ () {
        using namespace MyLSMTree;

        size_t kvs_cnt = 6400;
        size_t max_key_size = 3;
        size_t max_value_size = 20;

        for (size_t i = 0; i < 100; ++i) {
            std::mt19937 gen(i + 1);

            size_t fd_cache_size = 10;
            size_t sstable_scaling_factor = 5;
            size_t memtable_kv_count_limit = 100;
            size_t kv_buffer_slice_size = 1000;
            double filter_false_positive_rate = 0.1;
            Path tree_data = "tree_data.data";
            MyLSMTree::LSMTree tree(fd_cache_size, sstable_scaling_factor, memtable_kv_count_limit,
                                    kv_buffer_slice_size, filter_false_positive_rate, tree_data);

            std::map<Key, Value> map;
            std::vector<Key> keys;
            for (size_t j = 0; j < kvs_cnt; ++j) {
                size_t var = gen() % 4;
                switch (var) {
                    case 0: {  // insert
                        Key key = GenerateRandomKey(gen, max_key_size);
                        Value value = GenerateRandomValue(gen, max_value_size, false);
                        map[key] = value;
                        tree.Insert(key, value);
                        keys.emplace_back(std::move(key));
                        break;
                    }
                    case 1: {  // erase
                        Key key = GenerateRandomKey(gen, max_key_size);
                        map.erase(key);
                        tree.Erase(key);
                        keys.emplace_back(std::move(key));
                        break;
                    }
                    case 2: {  // find
                        size_t existing = keys.size() ? gen() % 2 : 0;
                        Key key;
                        if (existing) {
                            size_t ind = gen() % keys.size();
                            key = keys[ind];
                        } else {
                            key = GenerateRandomKey(gen, max_key_size);
                        }
                        auto tree_ans = tree.Find(key);
                        auto it = map.find(key);
                        if (it == map.end()) {
                            assert(!tree_ans.has_value());
                        } else {
                            assert(tree_ans.has_value());
                            assert(*tree_ans == it->second);
                        }
                        break;
                    }
                    case 3: {  // find range
                        size_t p = gen() % 16;
                        KeyRange range{.lower = std::nullopt,
                                       .upper = std::nullopt,
                                       .including_lower = (p & 1) != 0,
                                       .including_upper = (p & 2) != 0};
                        if (p & 4) {
                            range.lower = GenerateRandomKey(gen, max_key_size * 2);
                        }
                        if (p & 8) {
                            range.upper = GenerateRandomKey(gen, max_key_size * 2);
                        }
                        RangeLookupResult correct_answer;
                        for (const auto& kv : map) {
                            if (IsInRange(range, kv.first)) {
                                correct_answer[kv.first] = kv.second;
                            }
                        }

                        RangeLookupResult tree_answer = tree.FindRange(range);
                        assert(tree_answer == correct_answer);
                        break;
                    }
                }
            }

            std::cout << "Test_LSMTree_Correctnes_2 " << i << " OK" << std::endl;
        }
    },
    [] /*Test_LSMTree_Save_Load_Correctness*/ () {
        using namespace MyLSMTree;

        size_t kvs_cnt = 6400;
        size_t max_key_size = 3;
        size_t max_value_size = 20;

        for (size_t i = 0; i < 100; ++i) {
            std::mt19937 gen(i + 100);

            size_t fd_cache_size = 10;
            size_t sstable_scaling_factor = 5;
            size_t memtable_kv_count_limit = 100;
            size_t kv_buffer_slice_size = 1000;
            double filter_false_positive_rate = 0.1;
            Path tree_data = "tree_data.data";
            std::unique_ptr<MyLSMTree::LSMTree> tree =
                std::make_unique<MyLSMTree::LSMTree>(fd_cache_size, sstable_scaling_factor, memtable_kv_count_limit,
                                                     kv_buffer_slice_size, filter_false_positive_rate, tree_data);

            std::map<Key, Value> map;
            std::vector<Key> keys;
            for (size_t j = 0; j < kvs_cnt; ++j) {
                size_t var = gen() % 4;
                switch (var) {
                    case 0: {  // insert
                        Key key = GenerateRandomKey(gen, max_key_size);
                        Value value = GenerateRandomValue(gen, max_value_size, false);
                        map[key] = value;
                        tree->Insert(key, value);
                        keys.emplace_back(std::move(key));
                        break;
                    }
                    case 1: {  // erase
                        Key key = GenerateRandomKey(gen, max_key_size);
                        map.erase(key);
                        tree->Erase(key);
                        keys.emplace_back(std::move(key));
                        break;
                    }
                    case 2: {  // find
                        size_t existing = keys.size() ? gen() % 2 : 0;
                        Key key;
                        if (existing) {
                            size_t ind = gen() % keys.size();
                            key = keys[ind];
                        } else {
                            key = GenerateRandomKey(gen, max_key_size);
                        }
                        auto tree_ans = tree->Find(key);
                        auto it = map.find(key);
                        if (it == map.end()) {
                            assert(!tree_ans.has_value());
                        } else {
                            assert(tree_ans.has_value());
                            assert(*tree_ans == it->second);
                        }
                        break;
                    }
                    case 3: {  // find range
                        size_t p = gen() % 16;
                        KeyRange range{.lower = std::nullopt,
                                       .upper = std::nullopt,
                                       .including_lower = (p & 1) != 0,
                                       .including_upper = (p & 2) != 0};
                        if (p & 4) {
                            range.lower = GenerateRandomKey(gen, max_key_size * 2);
                        }
                        if (p & 8) {
                            range.upper = GenerateRandomKey(gen, max_key_size * 2);
                        }
                        RangeLookupResult correct_answer;
                        for (const auto& kv : map) {
                            if (IsInRange(range, kv.first)) {
                                correct_answer[kv.first] = kv.second;
                            }
                        }

                        RangeLookupResult tree_answer = tree->FindRange(range);
                        assert(tree_answer == correct_answer);
                        break;
                    }
                }
            }

            tree = nullptr;
            tree = std::make_unique<MyLSMTree::LSMTree>(tree_data);

            for (size_t j = 0; j < kvs_cnt; ++j) {
                size_t var = gen() % 4;
                switch (var) {
                    case 0: {  // insert
                        Key key = GenerateRandomKey(gen, max_key_size);
                        Value value = GenerateRandomValue(gen, max_value_size, false);
                        map[key] = value;
                        tree->Insert(key, value);
                        keys.emplace_back(std::move(key));
                        break;
                    }
                    case 1: {  // erase
                        Key key = GenerateRandomKey(gen, max_key_size);
                        map.erase(key);
                        tree->Erase(key);
                        keys.emplace_back(std::move(key));
                        break;
                    }
                    case 2: {  // find
                        size_t existing = keys.size() ? gen() % 2 : 0;
                        Key key;
                        if (existing) {
                            size_t ind = gen() % keys.size();
                            key = keys[ind];
                        } else {
                            key = GenerateRandomKey(gen, max_key_size);
                        }
                        auto tree_ans = tree->Find(key);
                        auto it = map.find(key);
                        if (it == map.end()) {
                            assert(!tree_ans.has_value());
                        } else {
                            assert(tree_ans.has_value());
                            assert(*tree_ans == it->second);
                        }
                        break;
                    }
                    case 3: {  // find range
                        size_t p = gen() % 16;
                        KeyRange range{.lower = std::nullopt,
                                       .upper = std::nullopt,
                                       .including_lower = (p & 1) != 0,
                                       .including_upper = (p & 2) != 0};
                        if (p & 4) {
                            range.lower = GenerateRandomKey(gen, max_key_size * 2);
                        }
                        if (p & 8) {
                            range.upper = GenerateRandomKey(gen, max_key_size * 2);
                        }
                        RangeLookupResult correct_answer;
                        for (const auto& kv : map) {
                            if (IsInRange(range, kv.first)) {
                                correct_answer[kv.first] = kv.second;
                            }
                        }

                        RangeLookupResult tree_answer = tree->FindRange(range);
                        assert(tree_answer == correct_answer);
                        break;
                    }
                }
            }

            std::cout << "Test_LSMTree_Save_Load_Correctness " << i << " OK" << std::endl;
        }
    }};

void Test_All() {
    for (const auto& test : tests) {
        test();
    }
}

void Test_Last() {
    tests.back()();
}

}  // namespace Test
