#include "odr/TranslationHelper.h"
#include "odr/TranslationConfig.h"
#include "OpenDocumentFile.h"
#include "Context.h"
#include "DocumentTranslator.h"

namespace odr {

class TranslationHelperImpl : public TranslationHelper {
public:
    std::unique_ptr<OpenDocumentFile> file;
    std::unique_ptr<DocumentTranslator> translator;

    explicit TranslationHelperImpl() {
        file = OpenDocumentFile::create();
        translator = DocumentTranslator::create();
    }
    ~TranslationHelperImpl() override = default;

    bool open(const std::string &in) override {
        return file->open(in);
    }

    bool translate(const std::string &out, const TranslationConfig &config) const override {
        Context context = {};
        context.config = &config;

        switch (file->getMeta().type) {
            case OpenDocumentFile::Meta::Type::TEXT:
            case OpenDocumentFile::Meta::Type::SPREADSHEET:
            case OpenDocumentFile::Meta::Type::PRESENTATION:
            case OpenDocumentFile::Meta::Type::GRAPHICS:
                // TODO: optimize; dont reload xml, dont regenerate styles, ... for same input file
                return translator->translate(*file, out, context);
            default:
                return false;
        }
    }
};

std::unique_ptr<TranslationHelper> TranslationHelper::create() {
    return std::make_unique<TranslationHelperImpl>();
}

}
