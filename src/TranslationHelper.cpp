#include "odr/TranslationHelper.h"
#include "tinyxml2.h"
#include "Constants.h"
#include "odr/FileMeta.h"
#include "odr/TranslationConfig.h"
#include "TranslationContext.h"
#include "io/Path.h"
#include "odf/OpenDocumentFile.h"
#include "odf/OpenDocumentTranslator.h"
#include "ooxml/MicrosoftOpenXmlFile.h"
#include "ooxml/MicrosoftTranslator.h"

namespace odr {

class TranslationHelper::Impl final {
public:
    OpenDocumentFile fileOd;
    MicrosoftOpenXmlFile fileMs;

    TranslationConfig config;
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

    const FileMeta *getMeta() const {
        if (fileOd.isOpen()) {
            return &fileOd.getMeta();
        } else if (fileMs.isOpen()) {
            return &fileMs.getMeta();
        }
        return nullptr;
    }

    bool decrypt(const std::string &password) {
        if (fileOd.isOpen()) {
            return fileOd.decrypt(password);
        } else if (fileMs.isOpen()) {
            return fileMs.decrypt(password);
        }
        return false;
    }

    bool translate(const std::string &outPath, const TranslationConfig &c) {
        config = c;
        context = {};
        context.config = &config;

        if (fileOd.isOpen()) {
            return translateOd(outPath);
        } else if (fileMs.isOpen()) {
            return translateMs(outPath);
        }
        return false;
    }

    bool translateOd(const std::string &outPath) {
        if (!fileOd.isDecrypted()) {
            return false;
        }

        context.odFile = &fileOd;
        context.meta = &fileOd.getMeta();

        switch (fileOd.getMeta().type) {
            case FileType::OPENDOCUMENT_TEXT:
            case FileType::OPENDOCUMENT_PRESENTATION:
            case FileType::OPENDOCUMENT_SPREADSHEET:
            case FileType::OPENDOCUMENT_GRAPHICS:
                // TODO: optimize; dont reload xml, dont regenerate styles, ... for same input file
                return translatorOd.translate(fileOd, outPath, context);
            default:
                return false;
        }
    }

    bool translateMs(const std::string &outPath) {
        if (!fileMs.isDecrypted()) {
            return false;
        }

        context.msFile = &fileMs;
        context.meta = &fileMs.getMeta();

        switch (fileMs.getMeta().type) {
            case FileType::OFFICE_OPEN_XML_DOCUMENT:
            case FileType::OFFICE_OPEN_XML_PRESENTATION:
            case FileType::OFFICE_OPEN_XML_WORKBOOK:
                // TODO: optimize; dont reload xml, dont regenerate styles, ... for same input file
                return translatorMs.translate(fileMs, outPath, context);
            default:
                return false;
        }
    }

    bool backTranslate(const std::string &in, const std::string &out) {
        if (fileOd.isOpen()) {
            return backTranslateOd(in, out);
        } else if (fileMs.isOpen()) {
            return backTranslateMs(in, out);
        }
        return false;
    }

    bool backTranslateOd(const std::string &diff, const std::string &outPath) {
        if (!context.config->editable) {
            return false;
        }

        switch (fileOd.getMeta().type) {
            case FileType::OPENDOCUMENT_TEXT:
            case FileType::OPENDOCUMENT_PRESENTATION:
            case FileType::OPENDOCUMENT_SPREADSHEET:
            case FileType::OPENDOCUMENT_GRAPHICS:
                return translatorOd.backTranslate(fileOd, diff, outPath, context);
            default:
                return false;
        }
    }

    bool backTranslateMs(const std::string &diff, const std::string &outPath) {
        if (!context.config->editable) {
            return false;
        }

        switch (fileMs.getMeta().type) {
            case FileType::OFFICE_OPEN_XML_DOCUMENT:
                return translatorMs.backTranslate(fileMs, diff, outPath, context);
            case FileType::OFFICE_OPEN_XML_PRESENTATION:
            case FileType::OFFICE_OPEN_XML_WORKBOOK:
            default:
                return false;
        }
    }
};

std::string TranslationHelper::getVersion() {
    return Constants::getVersion();
}

std::string TranslationHelper::getCommit() {
    return Constants::getCommit();
}

TranslationHelper::TranslationHelper() :
        impl_(std::make_unique<Impl>()) {
}

TranslationHelper::~TranslationHelper() = default;

bool TranslationHelper::openOpenDocument(const std::string &path) noexcept {
    try {
        return impl_->openOd(path);
    } catch (...) {
        return false;
    }
}

bool TranslationHelper::openMicrosoft(const std::string &path) noexcept {
    try {
        return impl_->openMs(path);
    } catch (...) {
        return false;
    }
}

void TranslationHelper::close() noexcept {
    impl_->close();
}

const FileMeta *TranslationHelper::getMeta() const noexcept {
    return impl_->getMeta();
}

bool TranslationHelper::decrypt(const std::string &password) noexcept {
    try {
        return impl_->decrypt(password);
    } catch (...) {
        return false;
    }
}

bool TranslationHelper::translate(const std::string &outPath, const TranslationConfig &config) noexcept {
    try {
        return impl_->translate(outPath, config);
    } catch (...) {
        return false;
    }
}

bool TranslationHelper::backTranslate(const std::string &diff, const std::string &outPath) noexcept {
    try {
        return impl_->backTranslate(diff, outPath);
    } catch (...) {
        return false;
    }
}

}
