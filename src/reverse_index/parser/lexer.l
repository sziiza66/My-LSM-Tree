
#include "lexer.h"
#include <string>

namespace MyLSMTree::ReverseIndex::BoolAst {

%%{
    machine bool_lexer;

    escape = '\\' any;
    operand_char = (any - [|&~() \t\r\n\\]) | escape;
    operand = operand_char+;

    main := |*
        [ \t\r\n]+;
        '&' => { ret = BoolParser::token::AND; fbreak; };
        '|' => { ret = BoolParser::token::OR; fbreak; };
        '~' => { ret = BoolParser::token::NOT; fbreak; };
        '(' => { ret = BoolParser::token::LPAR; fbreak; };
        ')' => { ret = BoolParser::token::RPAR; fbreak; };
        operand => {
            val->emplace<std::string>(std::string(ts, te - ts));
            ret = BoolParser::token::OPERAND;
            fbreak;
        };
    *|;
    write data;
}%%

Lexer::Lexer(const char* p, const char* pe)
    : p(p), pe(pe), eof(pe), s(p)
{
    %%write init;
}

BoolParser::token_type Lexer::lex(BoolParser::semantic_type* val, BoolParser::location_type* loc)
{
    BoolParser::token_type ret = BoolParser::token::END;
    %%write exec;

    loc->begin.column = ts - s;
    loc->end.column   = te - s;

    if (ret == BoolParser::token::END && p != pe && te != pe) {
        if (te) {
            std::cerr << "Unexpected input: \"" << std::string(te, pe) << "\"" << std::endl;
        } else {
            std::cerr << "Unexpected input: \"" << std::string(ts, pe) << "\"" << std::endl;
        }
        ret = BoolParser::token::ERROR;
    }

    return ret;
}

}