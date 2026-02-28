#pragma once

#include "spacy/spacy"

namespace MyLSMTree::ReverseIndex {

class NlpWrapper : public Spacy::Nlp {
public:
    NlpWrapper(Spacy::Nlp&& nlp);

    void DisableUnused();
};

}  // namespace MyLSMTree::ReverseIndex
