#include "nlp_wrapper.h"

namespace MyLSMTree::ReverseIndex {

NlpWrapper::NlpWrapper(Spacy::Nlp&& nlp) : Nlp(std::move(nlp)) {
    DisableUnused();
}

void NlpWrapper::DisableUnused() {
    using namespace Spacy;
    std::vector<PyObjectPtr> args;
    args.push_back(Python::get_object<std::string>("ner"));
    args.push_back(Python::get_object<std::string>("parser"));
    args.push_back(Python::get_object<std::string>("senter"));
    args.push_back(Python::get_object<std::string>("tok2vec"));

    Python::call_method<PyObjectPtr>(
        m_nlp,
        "disable_pipes",
        args
    );
}

}  // namespace MyLSMTree::ReverseIndex
