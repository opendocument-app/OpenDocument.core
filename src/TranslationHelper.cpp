#include "odr/TranslationHelper.h"
#include "odr/TranslationConfig.h"
#include "OpenDocumentFile.h"
#include "DocumentTranslator.h"

namespace odr {

class TranslationHelperImpl : public TranslationHelper {
public:
    std::unique_ptr<DocumentTranslator> translator;

    explicit TranslationHelperImpl(const TranslationConfig &config) {
        translator = DocumentTranslator::create(config);
    }
    ~TranslationHelperImpl() override = default;

    bool translate(const std::string &in, const std::string &out) const override {
        auto odf = OpenDocumentFile::create();
        odf->open(out);

        switch (odf->getMeta().type) {
            case OpenDocumentFile::Meta::Type::TEXT:
                return translator->translate(*odf, out);
            default:
                return false;
        }
    }
};

std::unique_ptr<TranslationHelper> TranslationHelper::create(const TranslationConfig &config) {
    return std::make_unique<TranslationHelperImpl>(config);
}

}
