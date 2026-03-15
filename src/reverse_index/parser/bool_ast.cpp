#include "bool_ast.h"
#include <cassert>
#include <limits>
#include <memory>

namespace MyLSMTree::ReverseIndex::BoolAst {

namespace {

AstPtr BitEquals(TokenId id_start, int bit_index, bool value) {
    TokenId id = value ? id_start + bit_index : id_start + 64 + bit_index;

    Operand op{id};
    return MakeArgNode(std::move(op));
}

AstPtr Clone(const Ast& node) {
    return std::visit(
        [](const auto& n) -> AstPtr {
            using T = std::decay_t<decltype(n)>;

            if constexpr (std::is_same_v<T, ArgNode>) {
                return MakeArgNode(n.GetOperand());

            } else if constexpr (std::is_same_v<T, NotNode>) {
                return MakeNotNode(Clone(n.GetOperand()));

            } else if constexpr (std::is_same_v<T, AndNode>) {
                return MakeAndNode(Clone(n.GetLeftOperand()), Clone(n.GetRightOperand()));

            } else if constexpr (std::is_same_v<T, OrNode>) {
                return MakeOrNode(Clone(n.GetLeftOperand()), Clone(n.GetRightOperand()));
            }
        },
        node);
}

}  // namespace

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

uint64_t ToUint64(const std::string& str) {
    if (str.empty()) {
        throw std::runtime_error("invalid uint64: empty string");
    }
    uint64_t value = 0;
    for (char c : str) {
        if (c < '0' || c > '9') {
            throw std::runtime_error("invalid uint64: non-digit");
        }
        uint64_t digit = c - '0';
        if (value > (std::numeric_limits<uint64_t>::max() - digit) / 10) {
            throw std::runtime_error("uint64 overflow");
        }
        value = value * 10 + digit;
    }
    return value;
}

AstPtr MakeNumericComparisonAst(TokenId id_start, uint64_t num, CompOp opcode) {
    AstPtr result;
    AstPtr prefix_equal;

    for (int i = 63; i >= 0; --i) {
        bool bit = (num >> i) & 1;
        AstPtr cmp;

        switch (opcode) {
            case CompOp::le:
            case CompOp::leq:
                if (bit) {
                    cmp = BitEquals(id_start, i, false);
                }
                break;

            case CompOp::ge:
            case CompOp::geq:
                if (!bit) {
                    cmp = BitEquals(id_start, i, true);
                }
                break;
        }

        if (cmp) {

            if (prefix_equal) {
                cmp = MakeAndNode(Clone(*prefix_equal), std::move(cmp));
            }

            if (!result) {
                result = std::move(cmp);
            } else {
                result = MakeOrNode(std::move(result), std::move(cmp));
            }
        }

        AstPtr eq = BitEquals(id_start, i, bit);

        if (!prefix_equal) {
            prefix_equal = std::move(eq);
        } else {
            prefix_equal = MakeAndNode(std::move(prefix_equal), std::move(eq));
        }
    }

    if (opcode == CompOp::leq || opcode == CompOp::geq) {

        if (result) {
            result = MakeOrNode(std::move(result), std::move(prefix_equal));
        } else {
            result = std::move(prefix_equal);
        }
    }

    return result;
}

}  // namespace MyLSMTree::ReverseIndex::BoolAst
