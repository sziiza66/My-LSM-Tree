#include "reverse_index.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "parser/parser_wrapper.h"
#include "roaring.hh"
#include "utf8/checked.h"

namespace MyLSMTree::ReverseIndex {

namespace {

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

}  // namespace

ReverseIndex::ReverseIndex(const Path& index_data, TreeConstructorProps index_props,
                           TreeConstructorProps dictionary_props, TreeConstructorProps documents_props)
    : index_(index_props, index_data / "index_data"),
      dictionary_(dictionary_props, index_data / "dictionary_data"),
      documents_(documents_props, index_data / "documents_data"),
      index_data_(index_data),
      token_count_(0),
      doc_count_(0) {
}

ReverseIndex::ReverseIndex(const Path& index_data)
    : index_(index_data / "index_data"),
      dictionary_(index_data / "dictionary_data"),
      documents_(index_data / "documents_data"),
      index_data_(index_data) {
    std::ifstream in(index_data_ / "index_metadata");
    in >> token_count_ >> doc_count_;
}

ReverseIndex::~ReverseIndex() {
    std::ofstream out(index_data_ / "index_metadata");
    out << token_count_ << ' ' << doc_count_;
}

void ReverseIndex::InsertDocument(const Path& doc_path) {
    std::string normal_doc_path = doc_path.lexically_normal().string();
    std::ifstream in(doc_path, std::ios::binary | std::ios::ate);
    if (!in) {
        throw std::runtime_error(std::string("Can't open file ") + normal_doc_path.c_str());
    }
    auto text_size = in.tellg();
    std::string text;
    text.resize(text_size);
    in.seekg(0);
    if (!in.read(&text[0], text_size)) {
        throw std::runtime_error(std::string("Can't read file ") + normal_doc_path.c_str());
    }

    Key key(sizeof(doc_count_));
    std::memcpy(key.data(), &doc_count_, sizeof(doc_count_));
    std::string doc_name = doc_path.filename();
    Value value(doc_name.begin(), doc_name.end());
    documents_.Insert(key, value);

    constexpr size_t chunk_size = 1024 * 1024 / 2;
    std::set<TokenId> token_ids;
    auto tolower = [](unsigned char c) { return std::tolower(c); };
    NlpWrapper nlp = spacy_.load("en_core_web_lg");
    for (size_t chunk_start = 0, chunk_end = 0; chunk_start < text.size(); chunk_start = chunk_end) {
        chunk_end = chunk_start + chunk_size < text.size() ? chunk_start + chunk_size : text.size();
        while (chunk_end > chunk_start && (text[chunk_end - 1] & 0xC0) == 0x80) {
            --chunk_end;
        }
        size_t word_end = chunk_end;
        while (word_end > chunk_start && !std::isspace(text[word_end - 1])) {
            --word_end;
        }
        chunk_end = word_end != chunk_start ? word_end : chunk_end;
        chunk_end = chunk_end == chunk_start
                        ? (chunk_start + chunk_size < text.size() ? chunk_start + chunk_size : text.size())
                        : chunk_end;
        std::string chunk =
            utf8::replace_invalid(std::string_view(text.data() + chunk_start, chunk_end - chunk_start), U' ');
        auto parsed = nlp.parse(chunk);
        for (const auto& token : parsed.tokens()) {
            if (token.is_punct() || token.is_space()) {
                continue;
            }
            auto lemma = token.lemma_();
            if (stop_words_.find(lemma) != stop_words_.end()) {
                continue;
            }
            std::transform(lemma.begin(), lemma.end(), lemma.begin(), tolower);
            token_ids.insert(GetTokenIdAndInsert(lemma));
        }
        // std::cout << "chunk done" << std::endl;
    }

    for (auto token_id : token_ids) {
        AssociateTokenWithDocument(token_id, doc_count_);
    }
    ++doc_count_;
}

std::vector<Path> ReverseIndex::LookupWithExpression(const std::string& query) const {
    NlpWrapper nlp = spacy_.load("en_core_web_lg");
    auto ast = BoolAst::ParseQuery(query, [&](const std::string& word) {
        auto parsed = nlp.parse(word);
        BoolAst::Operand res;
        for (const auto& token : parsed.tokens()) {
            if (token.is_punct() || token.is_space()) {
                continue;
            }
            auto lemma = token.lemma_();
            if (stop_words_.find(lemma) != stop_words_.end()) {
                continue;
            }
            std::transform(lemma.begin(), lemma.end(), lemma.begin(), tolower);
            auto token_id = GetTokenId(lemma);
            if (token_id) {
                res.emplace_back(*token_id);
            } else {
                res.emplace_back(token_count_);
                break;
            }
        }
        return res;
    });
    assert(ast);
    return LookupWithAst(*ast);
}

TokenId ReverseIndex::GetTokenIdAndInsert(const Token& token) {
    Key key = GetLSMKeyFromToken(token);
    if (auto res = dictionary_.Find(key); res.has_value()) {
        TokenId ret;
        std::memcpy(&ret, res->data(), sizeof(ret));
        return ret;
    }
    Value value(sizeof(token_count_));
    std::memcpy(value.data(), &token_count_, sizeof(token_count_));
    dictionary_.Insert(key, value);
    return token_count_++;
}

std::optional<TokenId> ReverseIndex::GetTokenId(const Token& token) const {
    Key key = GetLSMKeyFromToken(token);
    if (auto res = dictionary_.Find(key); res.has_value()) {
        TokenId ret;
        std::memcpy(&ret, res->data(), sizeof(ret));
        return ret;
    }
    return std::nullopt;
}

void ReverseIndex::AssociateTokenWithDocument(TokenId token_id, DocId doc_id) {
    Key key(sizeof(token_id));
    std::memcpy(key.data(), &token_id, sizeof(token_id));
    BitMap bitmap = GetBitmap(key);
    bitmap.add(doc_id);
    Value new_value(bitmap.getSizeInBytes());
    bitmap.write(reinterpret_cast<char*>(new_value.data()), true);
    // std::cout << "ADD: ";
    // for (uint8_t byte : new_value) {
    //     std::cout << static_cast<uint64_t>(byte) << ' ';
    // }
    // std::cout << "\n" << bitmap.cardinality() << "\n\n";
    index_.Insert(key, new_value);
}

Key ReverseIndex::GetLSMKeyFromToken(Token token) const {
    return Key(token.begin(), token.end());
}

std::vector<Path> ReverseIndex::LookupWithAst(const Ast& ast) const {
    BitMap whole = BitMap();
    whole.addRange(0, doc_count_);
    auto res = LookupBitmapWithAst(ast, whole);
    std::vector<Path> found_docs;
    Key key(sizeof(DocId));
    for (DocId id : res) {
        std::memcpy(key.data(), &id, sizeof(id));
        auto value = documents_.Find(key);
        if (value.has_value()) {
            found_docs.emplace_back(std::string(value->begin(), value->end()));
        }
    }
    return found_docs;
}

ReverseIndex::BitMap ReverseIndex::LookupBitmapWithAst(const Ast& ast, const BitMap& whole) const {
    return std::visit(
        Overloaded{[&](const ArgNode& node) {
                       Key key(sizeof(TokenId));
                       BitMap res = whole;
                       for (auto token_id : node.GetOperand()) {
                           std::memcpy(key.data(), &token_id, sizeof(token_id));
                           res &= GetBitmap(key);
                           if (res.isEmpty()) {
                               break;
                           }
                       }
                       return res;
                   },
                   [&](const NotNode& node) { return whole - LookupBitmapWithAst(node.GetOperand(), whole); },
                   [&](const AndNode& node) {
                       auto left = LookupBitmapWithAst(node.GetLeftOperand(), whole);
                       if (left.isEmpty()) {
                           return left;
                       }
                       return left & LookupBitmapWithAst(node.GetRightOperand(), whole);
                   },
                   [&](const OrNode& node) {
                       auto left = LookupBitmapWithAst(node.GetLeftOperand(), whole);
                       if (left.cardinality() == whole.cardinality()) {
                           return left;
                       }
                       return left | LookupBitmapWithAst(node.GetRightOperand(), whole);
                   }},
        ast);
}

ReverseIndex::BitMap ReverseIndex::GetBitmap(const Key& key) const {
    auto value = index_.Find(key);
    BitMap bitmap = BitMap();
    if (value.has_value()) {
        bitmap = BitMap::read(reinterpret_cast<const char*>(value->data()), true);
        // std::cout << "GET: ";
        // for (uint8_t byte : *value) {
        //     std::cout << static_cast<uint64_t>(byte) << ' ';
        // }
        // std::cout << '\n' << bitmap.cardinality() << "\n\n";
    }
    return bitmap;
}

const std::unordered_set<std::string> MyLSMTree::ReverseIndex::ReverseIndex::stop_words_ = {
    "a",  "an",  "the",   "and",   "or",    "but",    "if",    "then", "else", "in",   "on",    "at",
    "by", "for", "with",  "of",    "to",    "from",   "is",    "are",  "was",  "were", "be",    "been",
    "do", "can", "could", "would", "shall", "should", "might", "must", "this", "that", "these", "those"};

}  // namespace MyLSMTree::ReverseIndex
