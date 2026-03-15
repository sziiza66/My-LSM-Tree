#pragma once

#include <functional>
#include <string>

#include "bool_ast.h"

namespace MyLSMTree::ReverseIndex::BoolAst {

AstPtr ParseQuery(const std::string& query, std::function<Operand(const std::string&)> transform,
                  std::function<TokenId(const std::string&)> numeric_transform);

}  // namespace MyLSMTree::ReverseIndex::BoolAst
