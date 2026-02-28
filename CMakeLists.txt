cmake_minimum_required(VERSION 3.28)
project(My-LSM-Tree LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_BUILD_TYPE Release)
# set(CMAKE_BUILD_TYPE Debug)

# additional warnings
add_compile_options(-Wall -Wextra -Wpedantic)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -fomit-frame-pointer -ftree-vectorize -flto -mavx2 -mbmi2 -mpopcnt -mlzcnt -mfma -DNDEBUG")


message(CMAKE_CXX_FLAGS_RELEASE = ${CMAKE_CXX_FLAGS_RELEASE})

SET(SOURCES
    src/main.cpp
    src/lsm_tree/tests/bench.cpp
    src/lsm_tree/common.cpp
    src/lsm_tree/lsm_tree.cpp
    src/lsm_tree/sstable/sstable_reader.cpp
    src/lsm_tree/memtable/memtable.cpp
    src/lsm_tree/memtable/skip_list/skip_list.cpp
    src/lsm_tree/memtable/skip_list/kvbuffer.cpp
    src/lsm_tree/memtable/bloom_filter/bitset.cpp
    src/lsm_tree/memtable/bloom_filter/bloom_filter.cpp
    src/reverse_index/reverse_index.cpp
    src/reverse_index/nlp_wrapper.cpp
    src/reverse_index/parser/parser_wrapper.cpp
    src/reverse_index/parser/bool_ast.cpp)

add_executable(main ${SOURCES})

add_subdirectory(src/reverse_index/parser)
target_link_libraries(main PRIVATE reverse_index_parser)

include(cmake/third_party.cmake)
target_link_libraries(main PRIVATE xxHash::xxhash roaring spacy utf8cpp)
