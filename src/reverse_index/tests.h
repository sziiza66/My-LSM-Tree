#include <iostream>
#include <string>
#include <vector>
#include "reverse_index.h"

const MyLSMTree::Path large_index_dir = "large_index_dir";
const MyLSMTree::Path small_index_dir = "small_index_dir";

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
        index.InsertDocument(path, {});
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
    };

    std::cout << "Initialising index" << std::endl;
    MyLSMTree::ReverseIndex::ReverseIndex index(large_index_dir);

    RunLookups(index, queries);
}

void BuildSmallIndex() {
    MyLSMTree::TreeConstructorProps props = {
        .fd_cache_size = 10,
        .sstable_scaling_factor = 5,
        .memtable_kv_count_limit = 500000,
        .kv_buffer_slice_size = 50000000,
        .filter_false_positive_rate = 0.05,
    };
    std::cout << "Initialising index" << std::endl;
    MyLSMTree::ReverseIndex::ReverseIndex index(small_index_dir, props, props, props);
    MyLSMTree::Path docs_dir = small_index_dir / "documents";

    index.InsertDocument(docs_dir / "VENDETTA.txt", {
        {"lifetime_start_date", 0},
        {"lifetime_end_date", 100}
    });
    std::cout << "Inserted" << std::endl;

    index.InsertDocument(docs_dir / "THE VICAR OF TOURS.txt", {
        {"lifetime_start_date", 20},
        {"lifetime_end_date", 30}
    });
    std::cout << "Inserted" << std::endl;

    index.InsertDocument(docs_dir / "THE VALLEY OF THE MOON.txt", {
        {"lifetime_start_date", 80} // still alive
    });
    std::cout << "Inserted" << std::endl;

    index.InsertDocument(docs_dir / "The Underground City.txt", {
        {"lifetime_start_date", 10} // still alive
    });
    std::cout << "Inserted" << std::endl;

    index.InsertDocument(docs_dir / "THE TALISMAN.txt", {
        {"lifetime_start_date", 90},
        {"lifetime_end_date", 110}
    });
    std::cout << "Inserted" << std::endl;

    index.InsertDocument(docs_dir / "The Natural History of Selborne.txt", {
        {"lifetime_start_date", 44},
        {"lifetime_end_date", 88}
    });
    std::cout << "Inserted" << std::endl;

    index.InsertDocument(docs_dir / "The Idiot by Fyodor Dostoyevsky.txt", {
        {"lifetime_start_date", 100},
        {"lifetime_end_date", 200}
    });
    std::cout << "Inserted" << std::endl;

    index.InsertDocument(docs_dir / "Court Records__C.L. v. Epstein, No. 910-cv-80447 (S.D. Fla. 2010)_MASTER.txt", {
        {"lifetime_start_date", 90},
        {"lifetime_end_date", 150}
    });
    std::cout << "Inserted" << std::endl;

    index.InsertDocument(docs_dir / "chemistry-by-chapter.txt", {
        {"lifetime_start_date", 300},
        {"lifetime_end_date", 450}
    });
    std::cout << "Inserted" << std::endl;

    index.InsertDocument(docs_dir / "bible.txt", {
        {"lifetime_start_date", 60} // still alive
    });
    std::cout << "Inserted" << std::endl;

    std::cout << "Small index built." << std::endl;
}

void TestSmallIndex() {
    const std::vector<std::string> queries = {
        "lifetime_start_date < 15",
        "lifetime_start_date > 80",
        "lifetime_start_date > 80 & idiot",
        "lifetime_start_date > 100",
        "lifetime_start_date >= 100",
        "~(lifetime_end_date >= 0)", // find all alive
    };

    std::cout << "Initialising index" << std::endl;
    MyLSMTree::ReverseIndex::ReverseIndex index(small_index_dir);

    RunLookups(index, queries);
}

void LookupCreatedIn(const MyLSMTree::ReverseIndex::ReverseIndex& index, uint64_t range_start, uint64_t range_end) {
    auto str_start = std::to_string(range_start);
    auto str_end = std::to_string(range_end);
    std::string query = "lifetime_start_date >= " + str_start + " & " + "lifetime_start_date <= " + str_end;
    std::cout << "Looking for docs, created in [" << str_start << ", " << str_end << "]..." << std::endl;
    auto res = index.LookupWithExpression(query);
    std::cout << "Found " << res.size() << " documents:" << std::endl;
    for (const auto& path : res) {
        std::cout << path.c_str() << '\n';
    }
    std::cout << std::endl;
}

void LookupValidIn(const MyLSMTree::ReverseIndex::ReverseIndex& index, uint64_t range_start, uint64_t range_end) {
    auto str_start = std::to_string(range_start);
    auto str_end = std::to_string(range_end);
    std::string query = "lifetime_start_date <= " + str_end + " & " + "(lifetime_end_date > " + str_start + " | ~(lifetime_end_date >= 0))";
    std::cout << "Looking for docs, valid in [" << str_start << ", " << str_end << "]..." << std::endl;
    auto res = index.LookupWithExpression(query);
    std::cout << "Found " << res.size() << " documents:" << std::endl;
    for (const auto& path : res) {
        std::cout << path.c_str() << '\n';
    }
    std::cout << std::endl;
}

void TestSmallIndexCreatedRanges() {
    std::cout << "Initialising index" << std::endl;
    MyLSMTree::ReverseIndex::ReverseIndex index(small_index_dir);


    LookupCreatedIn(index, 33, 99);
    LookupCreatedIn(index, 60, 60);
    LookupCreatedIn(index, 0, 10000);
    LookupCreatedIn(index, 10, 50);
}

void TestSmallIndexValidRanges() {
    std::cout << "Initialising index" << std::endl;
    MyLSMTree::ReverseIndex::ReverseIndex index(small_index_dir);


    LookupValidIn(index, 33, 99);
    LookupValidIn(index, 60, 60);
    LookupValidIn(index, 0, 10000);
    LookupValidIn(index, 10, 50);
}