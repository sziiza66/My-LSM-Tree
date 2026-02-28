#include "parser.hpp"

namespace MyLSMTree::ReverseIndex::BoolAst {

class Lexer
{
public:
    Lexer(const char *p, const char *pe);

    BoolParser::token_type lex(BoolParser::semantic_type* val, BoolParser::location_type* loc);

private:
    const char *p, *pe, *eof, *ts, *te, *s;
    int cs, act;
};

}
