#include "odr/TranslationHelper.h"
#include "odr/TranslationConfig.h"
#include "OpenDocumentFile.h"
#include "Context.h"
#include "DocumentTranslator.h"

namespace odr {

class TranslationHelperImpl : public TranslationHelper {
public:
    const TranslationConfig config;
    std::unique_ptr<DocumentTranslator> translator;

    explicit TranslationHelperImpl(const TranslationConfig &config)
            : config(config) {
        translator = DocumentTranslator::create();
    }
    ~TranslationHelperImpl() override = default;

    bool translate(const std::string &in, const std::string &out) const override {
        auto odf = OpenDocumentFile::create();
        odf->open(in);

        Context context = {};
        context.config = &config;

        switch (odf->getMeta().type) {
            case OpenDocumentFile::Meta::Type::TEXT:
                return translator->translate(*odf, out, context);
            case OpenDocumentFile::Meta::Type::SPREADSHEET:
                return translator->translate(*odf, out, context);
            case OpenDocumentFile::Meta::Type::PRESENTATION:
                return translator->translate(*odf, out, context);
            case OpenDocumentFile::Meta::Type::GRAPHICS:
                return translator->translate(*odf, out, context);
            default:
                return false;
        }
    }
};

std::unique_ptr<TranslationHelper> TranslationHelper::create(const TranslationConfig &config) {
    return std::make_unique<TranslationHelperImpl>(config);
}

}
