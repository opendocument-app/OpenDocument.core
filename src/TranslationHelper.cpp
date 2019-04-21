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

    bool decrypt(const std::string &password) override {
        return file->decrypt(password);
    }

    const FileMeta &getMeta() const override {
        return file->getMeta();
    }

    bool translate(const std::string &out, const TranslationConfig &config) const override {
        if (!file->isOpen() || !file->isDecrypted()) {
            return false;
        }

        Context context = {};
        context.config = &config;
        context.file = file.get();
        context.meta = &getMeta();

        switch (file->getMeta().type) {
            case FileType::OPENDOCUMENT_TEXT:
            case FileType::OPENDOCUMENT_PRESENTATION:
            case FileType::OPENDOCUMENT_SPREADSHEET:
            case FileType::OPENDOCUMENT_GRAPHICS:
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
