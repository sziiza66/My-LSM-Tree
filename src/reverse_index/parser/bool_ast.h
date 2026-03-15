#pragma once

#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

#include "../common.h"

namespace MyLSMTree::ReverseIndex::BoolAst {

class ArgNode;
class NotNode;
class AndNode;
class OrNode;

using Ast = std::variant<ArgNode, NotNode, AndNode, OrNode>;
using AstPtr = std::unique_ptr<Ast>;
using Operand = std::vector<TokenId>;

class ArgNode {
public:
    ArgNode(Operand&& arg);

    const Operand& GetOperand() const;

private:
    Operand arg_;
};

class NotNode {
public:
    NotNode(AstPtr op);

    const Ast& GetOperand() const;

private:
    AstPtr op_;
};

class AndNode {
public:
    AndNode(AstPtr lop, AstPtr rop);

    const Ast& GetLeftOperand() const;
    const Ast& GetRightOperand() const;

private:
    AstPtr lop_;
    AstPtr rop_;
};

class OrNode {
public:
    OrNode(AstPtr lop, AstPtr rop);

    const Ast& GetLeftOperand() const;
    const Ast& GetRightOperand() const;

private:
    AstPtr lop_;
    AstPtr rop_;
};

AstPtr MakeArgNode(Operand arg);
AstPtr MakeNotNode(AstPtr op);
AstPtr MakeAndNode(AstPtr lop, AstPtr rop);
AstPtr MakeOrNode(AstPtr lop, AstPtr rop);

enum class CompOp {
    le,
    ge,
    leq,
    geq,
};

uint64_t ToUint64(const std::string& str);
AstPtr MakeNumericComparisonAst(TokenId id_start, uint64_t num, CompOp opcode);

}  // namespace MyLSMTree::ReverseIndex::BoolAst
