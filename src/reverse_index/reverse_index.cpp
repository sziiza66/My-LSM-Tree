#include "reverse_index.h"
#include <strings.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "common.h"
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

class PostingListIterator {
public:
    PostingListIterator(const std::vector<uint8_t>& list)
        : pos_ptr_(list.data() + 2 * sizeof(uint64_t))
        , doc_ptr_(list.data())
        , end_ptr_(list.data() + list.size()) {
        uint64_t pos_cnt;
        std::memcpy(&pos_cnt, doc_ptr_ + sizeof(uint64_t), sizeof(pos_cnt));
        pos_end_ptr_ = pos_ptr_ + pos_cnt * sizeof(uint32_t);
    }

    bool AdvanceDoc() {
        uint64_t pos_cnt;
        std::memcpy(&pos_cnt, doc_ptr_ + sizeof(uint64_t), sizeof(pos_cnt));
        const uint8_t* new_doc_ptr = doc_ptr_ + 2 * sizeof(uint64_t) + pos_cnt * sizeof(uint32_t);
        if (new_doc_ptr >= end_ptr_) {
            return false;
        }
        doc_ptr_ = new_doc_ptr;
        pos_ptr_ = doc_ptr_ + 2 * sizeof(uint64_t);
        std::memcpy(&pos_cnt, doc_ptr_ + sizeof(uint64_t), sizeof(pos_cnt));
        pos_end_ptr_ = pos_ptr_ + pos_cnt * sizeof(uint32_t);
        return true;
    }

    bool AdvancePos() {
        if (pos_ptr_ + sizeof(uint32_t) >= pos_end_ptr_) {
            return false;
        }
        pos_ptr_ += sizeof(uint32_t);
        return true;
    }

    uint64_t GetDocId() const {
        uint64_t doc_id;
        std::memcpy(&doc_id, doc_ptr_, sizeof(doc_id));
        return doc_id;
    }

    uint32_t GetPos() const {
        uint32_t pos;
        std::memcpy(&pos, pos_ptr_, sizeof(pos));
        return pos;
    }

private:
    const uint8_t* pos_ptr_;
    const uint8_t* doc_ptr_;
    const uint8_t* end_ptr_;
    const uint8_t* pos_end_ptr_;
};

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

void ReverseIndex::InsertDocuments(const std::vector<Path>& doc_paths) {
    NlpWrapper nlp = spacy_.load("en_core_web_lg");
    for (const auto& doc_path : doc_paths) {
        std::cout << "Insetring " << doc_path << std::endl;
        InsertDocumentInternal(doc_path, nlp);
    }
}

void ReverseIndex::InsertDocumentInternal(const Path& doc_path, NlpWrapper& nlp) {
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
    std::unordered_map<TokenId, std::vector<uint32_t>> pos_mapping;
    std::unordered_map<std::string, TokenId> ids_cache;
    auto tolower = [](unsigned char c) { return std::tolower(c); };
    for (size_t chunk_start = 0, chunk_end = 0, pos = 0; chunk_start < text.size(); chunk_start = chunk_end) {
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
            if (lemma == "about") {
                int x = 0;
            }
            TokenId tid;
            auto it = ids_cache.find(lemma);
            if (it == ids_cache.end()) {
                tid = GetTokenIdAndInsert(lemma);
                ids_cache[lemma] = tid;
                pos_mapping[tid] = {};
            } else {
                tid = it->second;
            }
            pos_mapping[tid].push_back(pos);
            ++pos;
        }
        // std::cout << "chunk done" << std::endl;
    }

    for (const auto& [token_id, positions] : pos_mapping) {
        // AssociateTokenWithDocument(token_id, doc_count_);
        if (token_id == 648) {
            int x = 0;
        }
        AppendPostingList(token_id, doc_count_, positions);
    }
    ++doc_count_;
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
    std::unordered_map<TokenId, std::vector<uint32_t>> pos_mapping;
    std::unordered_map<std::string, TokenId> ids_cache;
    auto tolower = [](unsigned char c) { return std::tolower(c); };
    NlpWrapper nlp = spacy_.load("en_core_web_lg");
    for (size_t chunk_start = 0, chunk_end = 0, pos = 0; chunk_start < text.size(); chunk_start = chunk_end) {
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
            if (lemma == "about") {
                int x = 0;
            }
            TokenId tid;
            auto it = ids_cache.find(lemma);
            if (it == ids_cache.end()) {
                tid = GetTokenIdAndInsert(lemma);
                ids_cache[lemma] = tid;
                pos_mapping[tid] = {};
            } else {
                tid = it->second;
            }
            pos_mapping[tid].push_back(pos);
            ++pos;
        }
        // std::cout << "chunk done" << std::endl;
    }

    for (const auto& [token_id, positions] : pos_mapping) {
        // AssociateTokenWithDocument(token_id, doc_count_);
        if (token_id == 648) {
            int x = 0;
        }
        AppendPostingList(token_id, doc_count_, positions);
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

void ReverseIndex::AppendPostingList(TokenId token_id, DocId doc_id, std::vector<uint32_t> positions) {
    Key key(sizeof(token_id));
    std::memcpy(key.data(), &token_id, sizeof(token_id));

    std::vector<uint8_t> new_posting_lists;
    auto posting_lists = index_.Find(key);
    size_t original_size = 0;
    if (posting_lists) {
        original_size = posting_lists->size();
        new_posting_lists = std::move(*posting_lists);
    } // 104

    size_t pos_cnt = positions.size();
    new_posting_lists.resize(original_size + sizeof(doc_id) + sizeof(pos_cnt) + sizeof(positions[0]) * positions.size());
    std::memcpy(&new_posting_lists[original_size], &doc_id, sizeof(doc_id));
    std::memcpy(&new_posting_lists[original_size + sizeof(doc_id)], &pos_cnt, sizeof(pos_cnt));
    std::memcpy(&new_posting_lists[original_size + sizeof(doc_id) + sizeof(pos_cnt)], positions.data(), sizeof(positions[0]) * positions.size());

    index_.Insert(key, new_posting_lists);
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
                        BitMap res = BitMap();
                        std::vector<std::vector<uint8_t>> lists;
                        std::vector<PostingListIterator> iters;
                        lists.reserve(node.GetOperand().size());
                        iters.reserve(node.GetOperand().size());
                        std::unordered_map<TokenId, size_t> ind_map;
                        for (auto token_id : node.GetOperand()) {
                            auto it = ind_map.find(token_id);
                            if (it == ind_map.end()) {
                                ind_map[token_id] = lists.size();
                                std::memcpy(key.data(), &token_id, sizeof(token_id));
                                auto posting = index_.Find(key);
                                if (posting) {
                                    lists.emplace_back(std::move(*posting));
                                } else {
                                    return res;
                                }
                                iters.emplace_back(lists.back());
                            } else {
                                iters.emplace_back(lists[it->second]);
                            }
                        }

                        DocId max_doc = iters[0].GetDocId();

                        auto AdvanceAllDocs = [&]() {
                            size_t max_cnt = 0;
                            for (size_t i = 0; ; i = (i + 1) % iters.size()) {
                                DocId nxt_doc = iters[i].GetDocId();
                                while (nxt_doc < max_doc) {
                                    if (!iters[i].AdvanceDoc()) {
                                        return false;
                                    }
                                    nxt_doc = iters[i].GetDocId();
                                }
                                if (nxt_doc == max_doc) {
                                    ++max_cnt;
                                } else {
                                    max_cnt = 1;
                                    max_doc = nxt_doc;
                                }
                                if (max_cnt == lists.size()) {
                                    return true;
                                }
                            }
                            assert(false);
                            return false;
                        };

                        auto FindPos = [&]() {
                            uint32_t max_pos = iters[0].GetPos();
                            size_t max_cnt = 0;
                            for (size_t i = 0; ; i = (i + 1) % iters.size()) {
                                uint32_t nxt_pos = iters[i].GetPos();
                                while (nxt_pos < max_pos + i) {
                                    if (!iters[i].AdvancePos()) {
                                        return false;
                                    }
                                    nxt_pos = iters[i].GetPos();
                                }
                                if (nxt_pos == max_pos + i) {
                                    ++max_cnt;
                                } else {
                                    max_cnt = 1;
                                    assert(nxt_pos >= i);
                                    max_pos = nxt_pos - i;
                                }
                                if (max_cnt == lists.size()) {
                                    return true;
                                }
                            }
                            assert(false);
                            return false;
                        };
                        
                        while (AdvanceAllDocs()) {
                            if (FindPos()) {
                                res.add(max_doc);
                            }
                            if (!iters[0].AdvanceDoc()) {
                                break;
                            }
                        }

                        return res;
                    },
                    [&](const NotNode& node) {
                        return whole - LookupBitmapWithAst(node.GetOperand(), whole);
                    },
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

const std::unordered_set<std::string> MyLSMTree::ReverseIndex::ReverseIndex::stop_words_ = {
    "a",  "an",  "the"
};

void ReverseIndex::PrintPostingLists(const std::vector<Token>& tokens) {
    Key key(sizeof(TokenId));
    for (const auto& token : tokens) {
        std::cout << token << " ---------------------------------------------------------------------\n";
        auto token_id = GetTokenId(token);
        if (token_id) {
            std::memcpy(key.data(), &*token_id, sizeof(*token_id));
            auto posting = index_.Find(key);
            if (posting) {
                PostingListIterator iter(*posting);
                do {
                    DocId doc_id = iter.GetDocId();
                    std::cout << doc_id << " _________________________\n";
                    do {
                        uint32_t pos = iter.GetPos();
                        std::cout << pos << '\n';
                    } while (iter.AdvancePos());
                } while (iter.AdvanceDoc());
            }
        }
    }
}

}  // namespace MyLSMTree::ReverseIndex
