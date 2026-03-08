#pragma once

#include <unordered_set>

#include "common.h"
#include "parser/bool_ast.h"
#include "../lsm_tree/lsm_tree.h"

#include "roaring.hh"
#include "utf8/core.h"

namespace MyLSMTree::ReverseIndex {

class ReverseIndex {
    using BitMap = roaring::Roaring64Map;
    using Ast = BoolAst::Ast;
    using ArgNode = BoolAst::ArgNode;
    using NotNode = BoolAst::NotNode;
    using AndNode = BoolAst::AndNode;
    using OrNode = BoolAst::OrNode;

public:
    ReverseIndex(const Path& index_data, TreeConstructorProps index_props, TreeConstructorProps dictionary_props,
                 TreeConstructorProps documents_props);
    explicit ReverseIndex(const Path& index_data);
    ~ReverseIndex();

    void InsertDocument(const Path& doc_path);
    std::vector<Path> LookupWithExpression(const std::string& query) const;
    std::vector<Path> LookupWithPrefix(const std::string& prefix) const;
    std::vector<Path> LookupWithWildcard(const std::string& wildcard) const;

private:
    std::set<NGram> CleanseTextAndGenerateNGrams(const std::string& text) const;
    bool iSPunctAllowed(char c) const;
    void FetchNGramsFromWord(std::set<NGram>& storage, const std::string& word) const;
    void FetchNGramsFromWildcard(std::set<NGram>& storage, const std::string& word) const;
    TokenId GetTokenIdAndInsert(const NGram& n_gram);
    TokenId GetTokenIdAndInsert(const Token& token);
    std::optional<TokenId> GetTokenId(const NGram& token) const;
    std::optional<TokenId> GetTokenId(const Token& token) const;
    void AssociateTokenWithDocument(TokenId token_id, DocId doc_id);
    Key GetLSMKeyFromToken(Token token) const;
    std::vector<Path> LookupWithAst(const Ast& ast) const;
    BitMap LookupBitmapWithAst(const Ast& ast, const BitMap& whole) const;
    BitMap GetBitmap(const Key& key) const;

private:
    LSMTree index_;
    LSMTree dictionary_;
    LSMTree documents_;
    Path index_data_;
    TokenId token_count_;
    DocId doc_count_;
    static const std::unordered_map<utf8::utfchar32_t, std::string> fold_map_;
};

}  // namespace MyLSMTree::ReverseIndex
