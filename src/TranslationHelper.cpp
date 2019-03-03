#include "opendocument/TranslationHelper.h"
#include "OpenDocumentFile.h"
#include "TextDocumentTranslator.h"

namespace opendocument {

class TranslationHelperImpl : public TranslationHelper {
public:
    TranslationConfig &getConfig() override;

    bool translate(const std::string &in, const std::string &out) const override;

private:
    TranslationConfig config_;

    TextDocumentTranslator textTranslator_;
};

TranslationConfig& TranslationHelperImpl::getConfig() {
    return config_;
}

bool TranslationHelperImpl::translate(const std::string &in, const std::string &out) const {
    OpenDocumentFile odf(in);

    switch (odf.getMeta().type) {
        case OpenDocumentFile::Meta::Type::TEXT:
            return textTranslator_.translate(odf, out);
        default:
            return false;
    }
}

TranslationHelper& TranslationHelper::instance() {
    static TranslationHelperImpl instance;
    return instance;
}

}
