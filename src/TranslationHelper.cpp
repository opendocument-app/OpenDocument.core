#include "odr/TranslationHelper.h"
#include "tinyxml2.h"
#include "odr/FileMeta.h"
#include "odr/TranslationConfig.h"
#include "TranslationContext.h"
#include "io/Path.h"
#include "odf/OpenDocumentFile.h"
#include "odf/OpenDocumentTranslator.h"
#include "ms/MicrosoftOpenXmlFile.h"
#include "ms/MicrosoftTranslator.h"

namespace odr {

class TranslationHelper::Impl final {
public:
    OpenDocumentFile fileOd;
    MicrosoftOpenXmlFile fileMs;

    TranslationContext context;

    OpenDocumentTranslator translatorOd;
    MicrosoftTranslator translatorMs;

    bool openOd(const std::string &path) {
        return fileOd.open(path);
    }

    bool openMs(const std::string &path) {
        return fileMs.open(path);
    }

    void close() {
        context = {};

        if (fileOd.isOpen()) {
            fileOd.close();
        } else if (fileMs.isOpen()) {
            fileMs.close();
        }
    }

    bool decrypt(const std::string &password) {
        if (fileOd.isOpen()) {
            return fileOd.decrypt(password);
        } else if (fileMs.isOpen()) {
            return fileMs.decrypt(password);
        }
        return false;
    }

    const FileMeta *getMeta() const {
        if (fileOd.isOpen()) {
            return &fileOd.getMeta();
        } else if (fileMs.isOpen()) {
            return &fileMs.getMeta();
        }
        return nullptr;
    }

    bool translate(const std::string &out, const TranslationConfig &config) {
        if (fileOd.isOpen()) {
            return translateOd(out, config);
        } else if (fileMs.isOpen()) {
            return translateMs(out, config);
        }
        return false;
    }

    bool translateOd(const std::string &out, const TranslationConfig &config) {
        if (!fileOd.isDecrypted()) {
            return false;
        }

        context = {};
        context.config = &config;
        context.odFile = &fileOd;
        context.meta = &fileOd.getMeta();

        switch (fileOd.getMeta().type) {
            case FileType::OPENDOCUMENT_TEXT:
            case FileType::OPENDOCUMENT_PRESENTATION:
            case FileType::OPENDOCUMENT_SPREADSHEET:
            case FileType::OPENDOCUMENT_GRAPHICS:
                // TODO: optimize; dont reload xml, dont regenerate styles, ... for same input file
                return translatorOd.translate(fileOd, out, context);
            default:
                return false;
        }
    }

    bool translateMs(const std::string &out, const TranslationConfig &config) {
        if (!fileMs.isDecrypted()) {
            return false;
        }

        context = {};
        context.config = &config;
        context.msFile = &fileMs;
        context.meta = &fileMs.getMeta();

        switch (fileMs.getMeta().type) {
            case FileType::OFFICE_OPEN_XML_DOCUMENT:
            case FileType::OFFICE_OPEN_XML_PRESENTATION:
            case FileType::OFFICE_OPEN_XML_WORKBOOK:
                // TODO: optimize; dont reload xml, dont regenerate styles, ... for same input file
                return translatorMs.translate(fileMs, out, context);
            default:
                return false;
        }
    }
};

TranslationHelper::TranslationHelper() :
        impl_(std::make_unique<Impl>()) {
}

TranslationHelper::~TranslationHelper() = default;

bool TranslationHelper::openOpenDocument(const std::string &path) noexcept {
    return impl_->openOd(path);
}

bool TranslationHelper::openMicrosoft(const std::string &path) noexcept {
    return impl_->openMs(path);
}

void TranslationHelper::close() noexcept {
    impl_->close();
}

bool TranslationHelper::decrypt(const std::string &password) noexcept {
    return impl_->decrypt(password);
}

const FileMeta *TranslationHelper::getMeta() const noexcept {
    return impl_->getMeta();
}

bool TranslationHelper::translate(const std::string &out, const TranslationConfig &config) noexcept {
    return impl_->translate(out, config);
}

}
