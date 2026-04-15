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

}  // namespace

void PrintPostingLists() {
    MyLSMTree::ReverseIndex::ReverseIndex index(large_index_dir);
    index.PrintPostingLists(
        {"about", "seven"}
    );
}

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
    // for (const auto& entry : std::filesystem::directory_iterator(docs_dir)) {
    //     if (!entry.is_regular_file())
    //         continue;
    //     auto path = entry.path();
    //     std::cout << "Inserting: " << path << std::endl;
    //     index.InsertDocument(path);
    //     std::cout << "Inserted." << std::endl;
    // }
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(docs_dir)) {
        if (!entry.is_regular_file())
            continue;
        paths.emplace_back(entry.path());
    }
    index.InsertDocuments(paths);
    std::cout << "Large index built." << std::endl;
}

void TestLargeIndex() {
    const std::vector<std::string> queries = {
        "About seven in the evening",
        "call it a day",
        "do not kill",
        "daily food",
        "do not kill & daily food",
        "do not kill | daily food",
        "daily food & ~(do not kill)"
    };

    std::cout << "Initialising index" << std::endl;
    MyLSMTree::ReverseIndex::ReverseIndex index(large_index_dir);

    RunLookups(index, queries);
}

