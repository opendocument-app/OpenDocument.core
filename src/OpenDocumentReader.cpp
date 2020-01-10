#include "odr/OpenDocumentReader.h"
#include <memory>
#include "tinyxml2.h"
#include "Constants.h"
#include "odr/FileMeta.h"
#include "odr/TranslationConfig.h"
#include "TranslationContext.h"
#include "io/Path.h"
#include "io/Storage.h"
#include "io/ZipStorage.h"
#include "odf/OpenDocumentMeta.h"
#include "odf/OpenDocumentTranslator.h"
#include "ooxml/MicrosoftTranslator.h"
#include "ooxml/OfficeOpenXmlMeta.h"

namespace odr {

class OpenDocumentReader::Impl final {
public:
    std::unique_ptr<Storage> storage;

    FileMeta meta;

    TranslationConfig config;
    TranslationContext context;

    OpenDocumentTranslator translatorOd;
    MicrosoftTranslator translatorMs;

    FileType guess(const std::string &path) noexcept {
        // TODO guess by file extension first
        if (!open(path)) return FileType::UNKNOWN;
        const FileType result = meta.type;
        close();
        return result;
    }

    bool open(const std::string &path) noexcept {
        try {
            try {
                storage = std::make_unique<ZipReader>(path);

                try {
                    meta = OpenDocumentMeta::parseFileMeta(*storage);
                    return true;
                } catch(NoOpenDocumentFileException &) {}
                try {
                    meta = OfficeOpenXmlMeta::parseFileMeta(*storage);
                    return true;
                } catch(NoOfficeOpenXmlFileException &) {}
            } catch(NoZipFileException &) {}

            // file detection failed
            return false;
        } catch(...) {
            // error occurred
            return false;
        }
    }

    void close() noexcept {
        storage.reset();
        context = {};
    }

    bool canTranslate() const noexcept {
        switch (meta.type) {
            case FileType::OPENDOCUMENT_TEXT:
            case FileType::OPENDOCUMENT_PRESENTATION:
            case FileType::OPENDOCUMENT_SPREADSHEET:
            case FileType::OPENDOCUMENT_GRAPHICS:
            case FileType::OFFICE_OPEN_XML_DOCUMENT:
            case FileType::OFFICE_OPEN_XML_PRESENTATION:
            case FileType::OFFICE_OPEN_XML_WORKBOOK:
                return true;
            default:
                return false;
        }
    }

    bool canBackTranslate() const noexcept {
        switch (meta.type) {
            case FileType::OPENDOCUMENT_TEXT:
            case FileType::OPENDOCUMENT_PRESENTATION:
            case FileType::OPENDOCUMENT_SPREADSHEET:
            case FileType::OPENDOCUMENT_GRAPHICS:
                return !meta.encrypted;
            default:
                return false;
        }
    }

    FileMeta getMeta() const noexcept {
        return meta;
    }

    bool decrypt(const std::string &password) noexcept {
        if (!meta.encrypted) return true;
        // TODO
        return false;
    }

    bool translate(const std::string &outPath, const TranslationConfig &c) noexcept {
        // TODO check if decrypted
        if (!canTranslate()) return false;

        config = c;
        context = {};
        context.config = &config;
        context.meta = &meta;
        context.storage = storage.get();

        try {
            switch (meta.type) {
                case FileType::OPENDOCUMENT_TEXT:
                case FileType::OPENDOCUMENT_PRESENTATION:
                case FileType::OPENDOCUMENT_SPREADSHEET:
                case FileType::OPENDOCUMENT_GRAPHICS:
                    // TODO: optimize; dont reload xml, dont regenerate styles, ... for same input file
                    return translatorOd.translate(outPath, context);
                case FileType::OFFICE_OPEN_XML_DOCUMENT:
                case FileType::OFFICE_OPEN_XML_PRESENTATION:
                case FileType::OFFICE_OPEN_XML_WORKBOOK:
                    // TODO: optimize; dont reload xml, dont regenerate styles, ... for same input file
                    return translatorMs.translate(outPath, context);
                default:
                    return false;
            }
        } catch(...) {}
        return false;
    }

    bool backTranslate(const std::string &diff, const std::string &outPath) noexcept {
        if (!context.config->editable) return false;
        if (!canBackTranslate()) return false;

        try {
            switch (meta.type) {
                case FileType::OPENDOCUMENT_TEXT:
                case FileType::OPENDOCUMENT_PRESENTATION:
                case FileType::OPENDOCUMENT_SPREADSHEET:
                case FileType::OPENDOCUMENT_GRAPHICS:
                    return translatorOd.backTranslate(diff, outPath, context);
                default:
                    return false;
            }
        } catch(...) {}
        return false;
    }
};

std::string OpenDocumentReader::getVersion() {
    return Constants::getVersion();
}

std::string OpenDocumentReader::getCommit() {
    return Constants::getCommit();
}

OpenDocumentReader::OpenDocumentReader() :
        impl_(std::make_unique<Impl>()) {
}

OpenDocumentReader::~OpenDocumentReader() = default;

FileType OpenDocumentReader::guess(const std::string &path) const noexcept {
    return impl_->guess(path);
}

bool OpenDocumentReader::open(const std::string &path) noexcept {
    return impl_->open(path);
}

void OpenDocumentReader::close() noexcept {
    impl_->close();
}

bool OpenDocumentReader::canTranslate() const noexcept {
    return impl_->canTranslate();
}

bool OpenDocumentReader::canBackTranslate() const noexcept {
    return impl_->canBackTranslate();
}

FileMeta OpenDocumentReader::getMeta() const noexcept {
    return impl_->getMeta();
}

bool OpenDocumentReader::decrypt(const std::string &password) noexcept {
    return impl_->decrypt(password);
}

bool OpenDocumentReader::translate(const std::string &outPath, const TranslationConfig &config) noexcept {
    return impl_->translate(outPath, config);
}

bool OpenDocumentReader::backTranslate(const std::string &diff, const std::string &outPath) noexcept {
    return impl_->backTranslate(diff, outPath);
}

}
