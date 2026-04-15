#pragma once

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
    ReverseIndex(const Path& index_data, TreeConstructorProps index_props, TreeConstructorProps dictionary_props,
                 TreeConstructorProps documents_props);
    explicit ReverseIndex(const Path& index_data);
    ~ReverseIndex();

    void InsertDocuments(const std::vector<Path>& doc_path);
    void InsertDocument(const Path& doc_path);
    std::vector<Path> LookupWithExpression(const std::string& query) const;
    void PrintPostingLists(const std::vector<Token>& tokens);

private:
    void InsertDocumentInternal(const Path& doc_path, NlpWrapper& nlp);
    TokenId GetTokenIdAndInsert(const Token& token);
    std::optional<TokenId> GetTokenId(const Token& token) const;
    void AssociateTokenWithDocument(TokenId token_id, DocId doc_id);
    void AppendPostingList(TokenId token_id, DocId doc_id, std::vector<uint32_t> positions);
    Key GetLSMKeyFromToken(Token token) const;
    std::vector<Path> LookupWithAst(const Ast& ast) const;
    BitMap LookupBitmapWithAst(const Ast& ast, const BitMap& whole) const;

private:
    LSMTree index_;
    LSMTree dictionary_;
    LSMTree documents_;
    Spacy::Spacy spacy_;
    // NlpWrapper nlp_;
    Path index_data_;
    TokenId token_count_;
    DocId doc_count_;
    static const std::unordered_set<std::string> stop_words_;
};

}  // namespace MyLSMTree::ReverseIndex
