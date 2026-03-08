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

    text = utf8::replace_invalid(std::string_view(text.data(), text.size()), U' ');
    auto n_grams = CleanseTextAndGenerateNGrams(text);

    for (const auto& n_gram : n_grams) {
        AssociateTokenWithDocument(GetTokenIdAndInsert(n_gram), doc_count_);
    }
    ++doc_count_;
}

std::vector<Path> ReverseIndex::LookupWithExpression(const std::string& query) const {
    auto ast = BoolAst::ParseQuery(query, [&](const std::string& word) {
        auto n_grams = CleanseTextAndGenerateNGrams(word);
        BoolAst::Operand res;
        res.reserve(n_grams.size());
        for (const auto& n_gram : n_grams) {
            auto token_id = GetTokenId(n_gram);
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

std::vector<Path> ReverseIndex::LookupWithPrefix(const std::string& prefix) const {
    std::string word = std::string{0x01} + prefix;
    std::set<NGram> n_grams;
    FetchNGramsFromWord(n_grams, word);
    BoolAst::Operand res;
    res.reserve(n_grams.size());
    for (const auto& n_gram : n_grams) {
        auto token_id = GetTokenId(n_gram);
        if (token_id) {
            res.emplace_back(*token_id);
        } else {
            res.emplace_back(token_count_);
            break;
        }
    }
    auto ast = BoolAst::MakeArgNode(std::move(res));
    return LookupWithAst(*ast);
}

std::vector<Path> ReverseIndex::LookupWithWildcard(const std::string& wildcard) const {
    std::string word = std::string{0x01} + wildcard + std::string{0x02};
    std::set<NGram> n_grams;
    FetchNGramsFromWildcard(n_grams, word);
    BoolAst::Operand res;
    res.reserve(n_grams.size());
    for (const auto& n_gram : n_grams) {
        auto token_id = GetTokenId(n_gram);
        if (token_id) {
            res.emplace_back(*token_id);
        } else {
            res.emplace_back(token_count_);
            break;
        }
    }
    auto ast = BoolAst::MakeArgNode(std::move(res));
    return LookupWithAst(*ast);
}

std::set<NGram> ReverseIndex::CleanseTextAndGenerateNGrams(const std::string& text) const {
    std::string correct_text = utf8::replace_invalid(std::string_view(text.data(), text.size()), U' ');
    auto it = utf8::iterator(correct_text.begin(), correct_text.begin(), correct_text.end());
    auto end = utf8::iterator(correct_text.end(), correct_text.begin(), correct_text.end());

    std::set<NGram> result;
    std::string word = {0x01};

    for (; it != end; ++it) {
        utf8::utfchar32_t cp = *it;

        if (cp < 128) {
            char c = static_cast<char>(cp);

            c = std::tolower(static_cast<unsigned char>(c));

            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || iSPunctAllowed(c)) {
                word.push_back(c);
                continue;
            }
        } else {
            auto it = fold_map_.find(cp);
            if (it != fold_map_.end()) {
                word += it->second;
                continue;
            }
        }

        if (word.size() > 3) {
            word.push_back(0x02);
            FetchNGramsFromWord(result, word);
        }
        word.clear();
        word.push_back(0x01);
    }
    if (word.size() > 3) {
        word.push_back(0x02);
        FetchNGramsFromWord(result, word);
    }

    return result;
}

void ReverseIndex::FetchNGramsFromWord(std::set<NGram>& storage, const std::string& word) const {
    for (size_t n_gram_size = 3; n_gram_size <= 6; ++n_gram_size) {
        for (size_t pos = 0; pos + n_gram_size <= word.size(); ++pos) {
            NGram gram(n_gram_size);
            std::memcpy(gram.data(), &word[pos], n_gram_size);
            storage.emplace(std::move(gram));
        }
    }
}

void ReverseIndex::FetchNGramsFromWildcard(std::set<NGram>& storage, const std::string& word) const {
    for (size_t n_gram_size = 3; n_gram_size <= 6; ++n_gram_size) {
        for (size_t pos = 0; pos + n_gram_size <= word.size(); ++pos) {
            bool skip = false;
            for (size_t i = pos; !skip && i < pos + n_gram_size; ++i) {
                skip = word[i] == '*';
            }
            if (skip) {
                continue;
            }
            NGram gram(n_gram_size);
            std::memcpy(gram.data(), &word[pos], n_gram_size);
            storage.emplace(std::move(gram));
        }
    }
}

bool ReverseIndex::iSPunctAllowed(char c) const {
    switch (c) {
        case '-':
        case '_':
        case '@':
        case '#':
        case '%':
        case '$':
        return true;
    }
    return false;
}

TokenId ReverseIndex::GetTokenIdAndInsert(const Key& n_gram) {
    if (auto res = dictionary_.Find(n_gram); res.has_value()) {
        TokenId ret;
        std::memcpy(&ret, res->data(), sizeof(ret));
        return ret;
    }
    Value value(sizeof(token_count_));
    std::memcpy(value.data(), &token_count_, sizeof(token_count_));
    dictionary_.Insert(n_gram, value);
    return token_count_++;
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

std::optional<TokenId> ReverseIndex::GetTokenId(const NGram& token) const {
    if (auto res = dictionary_.Find(token); res.has_value()) {
        TokenId ret;
        std::memcpy(&ret, res->data(), sizeof(ret));
        return ret;
    }
    return std::nullopt;
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

const std::unordered_map<utf8::utfchar32_t, std::string> MyLSMTree::ReverseIndex::ReverseIndex::fold_map_ = {
    {U'é', "e"}, {U'É', "e"}, {U'è', "e"},  {U'È', "e"},  {U'ê', "e"},  {U'Ê', "e"},  {U'ë', "e"},  {U'Ë', "e"},
    {U'á', "a"}, {U'Á', "a"}, {U'à', "a"},  {U'À', "a"},  {U'â', "a"},  {U'Â', "a"},  {U'ä', "a"},  {U'Ä', "a"},
    {U'í', "i"}, {U'Í', "i"}, {U'ï', "i"},  {U'Ï', "i"},  {U'ó', "o"},  {U'Ó', "o"},  {U'ö', "o"},  {U'Ö', "o"},
    {U'ø', "o"}, {U'Ø', "o"}, {U'ú', "u"},  {U'Ú', "u"},  {U'ü', "u"},  {U'Ü', "u"},  {U'ñ', "n"},  {U'Ñ', "n"},
    {U'ç', "c"}, {U'Ç', "c"}, {U'ß', "ss"}, {U'ẞ', "ss"}, {U'œ', "oe"}, {U'Œ', "oe"}, {U'æ', "ae"}, {U'Æ', "ae"}};

}  // namespace MyLSMTree::ReverseIndex
