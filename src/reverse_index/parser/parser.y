
%skeleton "lalr1.cc"
%require "2.5"
%locations
%defines
%define api.namespace {MyLSMTree::ReverseIndex::BoolAst}
%define api.value.type variant
%define api.parser.class {BoolParser}
%define parse.error verbose

%code requires {
#include <functional>
#include <memory>
#include <string>
#include "bool_ast.h"

namespace MyLSMTree::ReverseIndex::BoolAst {
    class Lexer;
}

using Ast = MyLSMTree::ReverseIndex::BoolAst::Ast;
using namespace MyLSMTree::ReverseIndex::BoolAst;

}

%parse-param {MyLSMTree::ReverseIndex::BoolAst::Lexer& lexer} {std::unique_ptr<Ast>& result} {std::string& message} {std::function<MyLSMTree::ReverseIndex::BoolAst::Operand(const std::string&)> transform} {std::function<MyLSMTree::ReverseIndex::TokenId(const std::string&)> numeric_transform}

%code {
    #include "lexer.h"
    #define yylex lexer.lex
}

%token END 0 "end of file"
%token ERROR

%token LPAR
%token RPAR
%token AND
%token OR
%token NOT
%token LE "<"
%token LEQ "<="
%token GE ">"
%token GEQ ">="
%token <std::string> OPERAND

%type <AstPtr> expr
// %type <AstPtr> or_expr
// %type <AstPtr> and_expr
// %type <AstPtr> unary
// %type <AstPtr> primary

%left OR
%left AND
%right NOT

%left LE LEQ GE GEQ

%%

input: expr { result = std::move($1); }

expr: OPERAND { $$ = MakeArgNode(transform($1)); }
    | OPERAND LE OPERAND { $$ = MakeNumericComparisonAst(numeric_transform($1), ToUint64($3), CompOp::le); }
    | OPERAND GE OPERAND { $$ = MakeNumericComparisonAst(numeric_transform($1), ToUint64($3), CompOp::ge); }
    | OPERAND LEQ OPERAND { $$ = MakeNumericComparisonAst(numeric_transform($1), ToUint64($3), CompOp::leq); }
    | OPERAND GEQ OPERAND { $$ = MakeNumericComparisonAst(numeric_transform($1), ToUint64($3), CompOp::geq); }
    | LPAR expr RPAR { $$ = std::move($2); }
    | NOT expr { $$ = MakeNotNode(std::move($2)); }
    | expr AND expr { $$ = MakeAndNode(std::move($1), std::move($3)); }
    | expr OR expr { $$ = MakeOrNode(std::move($1), std::move($3)); }

%%

void MyLSMTree::ReverseIndex::BoolAst::BoolParser::error(const location_type& loc, const std::string& err)
{
    message = err;
}