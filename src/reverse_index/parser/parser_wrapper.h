#pragma once

#include <functional>
#include <string>

#include "bool_ast.h"

namespace MyLSMTree::ReverseIndex::BoolAst {

AstPtr ParseQuery(const std::string& query, std::function<Operand(std::string&)> transform);

}  // namespace MyLSMTree::ReverseIndex::BoolAst
