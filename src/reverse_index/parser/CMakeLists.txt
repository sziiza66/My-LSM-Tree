# =========================================
# GENERATED DIR
# =========================================
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

# =========================================
# BISON
# =========================================
find_package(BISON REQUIRED)

BISON_TARGET(
    BoolParser
    ${CMAKE_CURRENT_SOURCE_DIR}/parser.y
    ${GENERATED_DIR}/parser.cpp
    DEFINES_FILE ${GENERATED_DIR}/parser.hpp
)

# =========================================
# RAGEL
# =========================================
find_program(RAGEL_EXECUTABLE ragel REQUIRED)

add_custom_command(
    OUTPUT ${GENERATED_DIR}/lexer.cpp
    COMMAND ${RAGEL_EXECUTABLE}
        -C
        -o ${GENERATED_DIR}/lexer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/lexer.l
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/lexer.l
)

add_custom_target(ragel_lexer DEPENDS ${GENERATED_DIR}/lexer.cpp)

# =========================================
# LIB
# =========================================
add_library(reverse_index_parser
    ${BISON_BoolParser_OUTPUTS}
    ${GENERATED_DIR}/lexer.cpp
    parser_wrapper.cpp
)

add_dependencies(reverse_index_parser ragel_lexer)

target_include_directories(reverse_index_parser PUBLIC
    ${GENERATED_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(reverse_index_parser PUBLIC cxx_std_20)