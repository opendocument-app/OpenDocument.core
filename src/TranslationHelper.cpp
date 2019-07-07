#include "odr/TranslationHelper.h"
#include "odr/TranslationConfig.h"
#include "OpenDocumentFile.h"
#include "TranslationContext.h"
#include "OpenDocumentTranslator.h"
#include "MicrosoftOpenXmlFile.h"
#include "MicrosoftTranslator.h"

namespace odr {

class TranslationHelperImpl final {
public:
    OpenDocumentFile file;
    std::unique_ptr<OpenDocumentTranslator> translator;
    MicrosoftOpenXmlFile fileMs;
    std::unique_ptr<MicrosoftTranslator> translatorMs;

    TranslationHelperImpl() {
        translator = OpenDocumentTranslator::create();
        translatorMs = MicrosoftTranslator::create();
    }

    bool translate(const std::string &out, const TranslationConfig &config) {
        if (!file.isOpen() || !file.isDecrypted()) {
            return translateMs(out, config);
        }

        TranslationContext context = {};
        context.config = &config;
        context.odFile = &file;
        context.meta = &file.getMeta();

        switch (file.getMeta().type) {
            case FileType::OPENDOCUMENT_TEXT:
            case FileType::OPENDOCUMENT_PRESENTATION:
            case FileType::OPENDOCUMENT_SPREADSHEET:
            case FileType::OPENDOCUMENT_GRAPHICS:
                // TODO: optimize; dont reload xml, dont regenerate styles, ... for same input file
                return translator->translate(file, out, context);
            default:
                return false;
        }
    }

    bool translateMs(const std::string &out, const TranslationConfig &config) {
        if (!fileMs.isOpen() || !fileMs.isDecrypted()) {
            return false;
        }

        TranslationContext context = {};
        context.config = &config;
        context.msFile = &fileMs;
        context.meta = &fileMs.getMeta();

        switch (fileMs.getMeta().type) {
            case FileType::OFFICE_OPEN_XML_DOCUMENT:
            case FileType::OFFICE_OPEN_XML_PRESENTATION:
            case FileType::OFFICE_OPEN_XML_WORKBOOK:
                // TODO: optimize; dont reload xml, dont regenerate styles, ... for same input file
                return translatorMs->translate(fileMs, out, context);
            default:
                return false;
        }
    }
};

TranslationHelper::TranslationHelper() :
        impl_(new TranslationHelperImpl()) {
}

TranslationHelper::~TranslationHelper() {
    delete impl_;
}

bool TranslationHelper::open(const std::string &path) {
    return impl_->file.open(path);
}

bool TranslationHelper::openMicrosoft(const std::string &path) {
    return impl_->fileMs.open(path);
}

bool TranslationHelper::decrypt(const std::string &password) {
    return impl_->file.decrypt(password);
}

const FileMeta& TranslationHelper::getMeta() const {
    return impl_->file.getMeta();
}

bool TranslationHelper::translate(const std::string &out, const odr::TranslationConfig &config) {
    return impl_->translate(out, config);
}

}
