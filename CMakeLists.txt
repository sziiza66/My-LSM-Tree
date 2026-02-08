cmake_minimum_required(VERSION 3.28)
project(My-LSM-Tree LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# set(CMAKE_BUILD_TYPE Release)
set(CMAKE_BUILD_TYPE Debug)

if (MSVC)
    # warning level 4
    add_compile_options(/W4)
    set(CMAKE_CXX_FLAGS_DEBUG "/Od")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2")
else()
    # additional warnings
    add_compile_options(-Wall -Wextra -Wpedantic)
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    if (${CMAKE_CXX_COMPILER_ID} STREQUAL GNU)
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -msse2 -flto")
        # set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -fomit-frame-pointer -fno-semantic-interposition -ftree-vectorize -flto -unroll-loops -mavx2 -mbmi -mbmi2 -mpopcnt -mlzcnt -mfma -mabm")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "-O2 -msse2")
    endif()
endif()
message(CMAKE_CXX_FLAGS_RELEASE = ${CMAKE_CXX_FLAGS_RELEASE})

SET(SOURCES
    src/main.cpp
    src/lsm_tree/memtable/memtable.cpp
    src/lsm_tree/memtable/skip_list/skip_list.cpp
    src/lsm_tree/memtable/skip_list/kvbuffer.cpp
    src/lsm_tree/memtable/bloom_filter/bitset.cpp
    src/lsm_tree/memtable/bloom_filter/bloom_filter.cpp)

add_executable(main ${SOURCES})

set(XXHASH_BUILD_XXHSUM OFF)        # Don't build command line tool
option(BUILD_SHARED_LIBS OFF)       # Build static library

add_subdirectory(xxhash/build/cmake xxhash_build EXCLUDE_FROM_ALL)
target_link_libraries(main PRIVATE xxHash::xxhash)

