#include "bool_ast.h"
#include <cassert>
#include <memory>

namespace MyLSMTree::ReverseIndex::BoolAst {

ArgNode::ArgNode(Operand&& arg) : arg_(std::move(arg)) {
}

const Operand& ArgNode::GetOperand() const {
    return arg_;
}

NotNode::NotNode(AstPtr op) : op_(std::move(op)) {
}

const Ast& NotNode::GetOperand() const {
    return *op_;
}

AndNode::AndNode(AstPtr lop, AstPtr rop) : lop_(std::move(lop)), rop_(std::move(rop)) {
}

const Ast& AndNode::GetLeftOperand() const {
    return *lop_;
}

const Ast& AndNode::GetRightOperand() const {
    return *rop_;
}

OrNode::OrNode(AstPtr lop, AstPtr rop) : lop_(std::move(lop)), rop_(std::move(rop)) {
}

const Ast& OrNode::GetLeftOperand() const {
    return *lop_;
}

const Ast& OrNode::GetRightOperand() const {
    return *rop_;
}

std::unique_ptr<Ast> MakeArgNode(Operand arg) {
    return std::make_unique<Ast>(std::in_place_type<ArgNode>, std::move(arg));
}

std::unique_ptr<Ast> MakeNotNode(std::unique_ptr<Ast> op) {
    assert(op);
    return std::make_unique<Ast>(std::in_place_type<NotNode>, std::move(op));
}

std::unique_ptr<Ast> MakeAndNode(std::unique_ptr<Ast> lop, std::unique_ptr<Ast> rop) {
    assert(lop);
    assert(rop);
    return std::make_unique<Ast>(std::in_place_type<AndNode>, std::move(lop), std::move(rop));
}

std::unique_ptr<Ast> MakeOrNode(std::unique_ptr<Ast> lop, std::unique_ptr<Ast> rop) {
    assert(lop);
    assert(rop);
    return std::make_unique<Ast>(std::in_place_type<OrNode>, std::move(lop), std::move(rop));
}

}  // namespace MyLSMTree::ReverseIndex::BoolAst
