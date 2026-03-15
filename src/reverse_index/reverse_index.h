#pragma once

#include <limits>
#include <map>
#include <unordered_set>

#include "common.h"
#include "parser/bool_ast.h"
#include "../lsm_tree/lsm_tree.h"

#include "nlp_wrapper.h"
#include "roaring.hh"

namespace MyLSMTree::ReverseIndex {

class ReverseIndex {
    using BitMap = roaring::Roaring64Map;
    using Ast = BoolAst::Ast;
    using ArgNode = BoolAst::ArgNode;
    using NotNode = BoolAst::NotNode;
    using AndNode = BoolAst::AndNode;
    using OrNode = BoolAst::OrNode;

public:
    struct NumericTag {
        std::string name;
        uint64_t value;
    };
    using NumericTags = std::vector<NumericTag>;

    ReverseIndex(const Path& index_data, TreeConstructorProps index_props, TreeConstructorProps dictionary_props,
                 TreeConstructorProps documents_props);
    explicit ReverseIndex(const Path& index_data);
    ~ReverseIndex();

    void InsertDocument(const Path& doc_path, const NumericTags& num_tags);
    std::vector<Path> LookupWithExpression(const std::string& query) const;

private:
    TokenId GetFreeNumericIdStart() const;
    TokenId GetTokenIdAndInsert(const Token& token);
    std::optional<TokenId> GetTokenId(const Token& token) const;
    void AssociateTokenWithDocument(TokenId token_id, DocId doc_id);
    void AssociateNumericTagWithDocument(TokenId tag_id_start, uint64_t tag_value, DocId doc_id);
    Key GetLSMKeyFromToken(Token token) const;
    std::vector<Path> LookupWithAst(const Ast& ast) const;
    BitMap LookupBitmapWithAst(const Ast& ast, const BitMap& whole) const;
    BitMap GetBitmap(const Key& key) const;

private:
    LSMTree index_;
    LSMTree dictionary_;
    LSMTree documents_;
    std::map<std::string, TokenId> numeric_tags;
    Spacy::Spacy* spacy_;
    // NlpWrapper nlp_;
    Path index_data_;
    TokenId token_count_;
    DocId doc_count_;
    static const std::unordered_set<std::string> stop_words_;
    static constexpr TokenId emtpy_numeric_tag = std::numeric_limits<TokenId>::max() - 127;
};

Spacy::Spacy* GetSpacyInstance();

}  // namespace MyLSMTree::ReverseIndex
