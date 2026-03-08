#include <iostream>
#include <vector>
#include "reverse_index.h"

const MyLSMTree::Path large_index_dir = "large_index_dir";

namespace {

void RunLookups(const MyLSMTree::ReverseIndex::ReverseIndex& index, const std::vector<std::string>& queries) {
    for (const auto& query : queries) {
        std::cout << "Searching for: \"" << query << "\"..." << std::endl;
        auto res = index.LookupWithExpression(query);
        std::cout << "Found " << res.size() << " documents:" << std::endl;
        for (const auto& path : res) {
            std::cout << path.c_str() << '\n';
        }
        std::cout << std::endl;
    }
}

void RunPrefixLookups(const MyLSMTree::ReverseIndex::ReverseIndex& index, const std::vector<std::string>& queries) {
    for (const auto& query : queries) {
        std::cout << "Searching for: \"" << query << "\"..." << std::endl;
        auto res = index.LookupWithPrefix(query);
        std::cout << "Found " << res.size() << " documents:" << std::endl;
        for (const auto& path : res) {
            std::cout << path.c_str() << '\n';
        }
        std::cout << std::endl;
    }
}

void RunWildcardLookups(const MyLSMTree::ReverseIndex::ReverseIndex& index, const std::vector<std::string>& queries) {
    for (const auto& query : queries) {
        std::cout << "Searching for: \"" << query << "\"..." << std::endl;
        auto res = index.LookupWithWildcard(query);
        std::cout << "Found " << res.size() << " documents:" << std::endl;
        for (const auto& path : res) {
            std::cout << path.c_str() << '\n';
        }
        std::cout << std::endl;
    }
}

}  // namespace

void BuildLargeIndex() {
    MyLSMTree::TreeConstructorProps props = {
        .fd_cache_size = 10,
        .sstable_scaling_factor = 5,
        .memtable_kv_count_limit = 500000,
        .kv_buffer_slice_size = 50000000,
        .filter_false_positive_rate = 0.05,
    };
    std::cout << "Initialising index" << std::endl;
    MyLSMTree::ReverseIndex::ReverseIndex index(large_index_dir, props, props, props);
    MyLSMTree::Path docs_dir = large_index_dir / "documents";
    for (const auto& entry : std::filesystem::directory_iterator(docs_dir)) {
        if (!entry.is_regular_file())
            continue;
        auto path = entry.path();
        std::cout << "Inserting: " << path << std::endl;
        index.InsertDocument(path);
        std::cout << "Inserted." << std::endl;
    }
    std::cout << "Large index built." << std::endl;
}

void TestLargeIndex() {
    const std::vector<std::string> queries = {
        "Epstein",
        "Tsarsko-Selski & idiot",
        "Epstein | (Tsarsko-Selski & idiot)",
        "chemistry & catalyst",
        "science & theorems",
        "(science & theorems) | chemistry",
        "(science & theorems) & ~chemistry",
        "(politics & war & society & history) & (USSR | Russia)",
        "(politics & war & society & history) & ~(USSR | Russia)",

        "2:11 & For & he & that & said, & Do & not & commit & adultery, & said & also, & "\
        "Do & not & kill. & Now & if & thou & commit & no & adultery, & yet & if & "\
        "thou & kill, & thou & art & become & a & transgressor & of & the & law.",

        "1999 | 2000 | 1901 | 1990",
        ""
    };

    std::cout << "Initialising index" << std::endl;
    MyLSMTree::ReverseIndex::ReverseIndex index(large_index_dir);

    RunLookups(index, queries);
}

void TestPrefixSearchLargeIndex() {
    const std::vector<std::string> queries = {
        "tsar",
        "electro",
        "pseudo",
        "pseudon",
        // "tra",
        // "tran",
        // "trans",
        // "transf"
        "nebu",
        "nebul",
        "nebuc",
        "http",
        "https",
        "$10",
        "$100",
    };

    std::cout << "Initialising index" << std::endl;
    MyLSMTree::ReverseIndex::ReverseIndex index(large_index_dir);

    RunPrefixLookups(index, queries);
}

void TestWildcardSearchLargeIndex() {
    const std::vector<std::string> queries = {
        "552*171",
        "62*541",
        "characteri*ation" // characterisation or characterization
    };

    std::cout << "Initialising index" << std::endl;
    MyLSMTree::ReverseIndex::ReverseIndex index(large_index_dir);

    RunWildcardLookups(index, queries);
}
