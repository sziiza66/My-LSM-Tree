#include "parser_wrapper.h"
#include "parser.hpp"
#include "lexer.h"

extern void SetLexerInput(const char* data, size_t len);

namespace MyLSMTree::ReverseIndex::BoolAst {

AstPtr ParseQuery(const std::string& query, std::function<Operand(const std::string&)> transform,
                  std::function<TokenId(const std::string&)> numeric_transform) {

    AstPtr result;
    std::string err;
    Lexer lexer(query.c_str(), query.c_str() + query.size());
    BoolParser parser(lexer, result, err, transform, numeric_transform);

    if (!err.empty()) {
        throw std::runtime_error(err);
    }
    if (parser.parse() != 0) {
        throw std::runtime_error("Query parse error");
    }

    return result;
}

}  // namespace MyLSMTree::ReverseIndex::BoolAst
