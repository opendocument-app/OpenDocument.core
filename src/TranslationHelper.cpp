#include "opendocument/TranslationHelper.h"
#include "OpenDocumentFile.h"
#include "DocumentTranslator.h"

namespace opendocument {

class TranslationHelperImpl : public TranslationHelper {
public:
    TranslationConfig config_;
    std::unique_ptr<DocumentTranslator> translator_;

    ~TranslationHelperImpl() override = default;

    TranslationConfig& getConfig() override {
        return config_;
    }

    bool translate(const std::string &in, const std::string &out) const override {
        OpenDocumentFile odf(in);

        switch (odf.getMeta().type) {
            case OpenDocumentFile::Meta::Type::TEXT:
                return translator_->translate(odf, out);
            default:
                return false;
        }
    }
};

TranslationHelper& TranslationHelper::instance() {
    static TranslationHelperImpl instance;
    return instance;
}

}
