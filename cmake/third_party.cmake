set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

########################################
# xxHash
########################################

set(XXHASH_BUILD_XXHSUM OFF)        # Don't build command line tool
option(BUILD_SHARED_LIBS OFF)       # Build static library

add_subdirectory(${THIRD_PARTY_DIR}/xxHash/build/cmake xxhash_build EXCLUDE_FROM_ALL)

########################################
# CRoaring (amalgamation)
########################################

set(ROARING_DIR ${THIRD_PARTY_DIR}/CRoaring)
set(ROARING_GEN_DIR ${CMAKE_BINARY_DIR}/generated/roaring)

set(ROARING_C  ${ROARING_GEN_DIR}/roaring.c)
set(ROARING_H  ${ROARING_GEN_DIR}/roaring.h)
set(ROARING_HH ${ROARING_GEN_DIR}/roaring.hh)

add_custom_command(
    OUTPUT ${ROARING_C} ${ROARING_H} ${ROARING_HH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ROARING_GEN_DIR}
    COMMAND ${ROARING_DIR}/amalgamation.sh ${ROARING_GEN_DIR}
    WORKING_DIRECTORY ${ROARING_DIR}
    DEPENDS ${ROARING_DIR}/amalgamation.sh
    COMMENT "Generating CRoaring amalgamation"
)

add_custom_target(generate_roaring DEPENDS ${ROARING_C} ${ROARING_H} ${ROARING_HH})

add_library(roaring STATIC ${ROARING_C})

add_dependencies(roaring generate_roaring)

target_include_directories(roaring PUBLIC ${ROARING_GEN_DIR})

########################################
# utfcpp
########################################

add_subdirectory(${THIRD_PARTY_DIR}/utfcpp)
