#include "MicrosoftOpenXmlFile.h"
#include <unordered_map>
#include "ZipStorage.h"

namespace odr {

MicrosoftOpenXmlFile::MicrosoftOpenXmlFile(const Path &path, const std::string &password) {
    // TODO decryption

    parent = std::make_unique<ZipReader>(path);

    generateMeta();
}

MicrosoftOpenXmlFile::MicrosoftOpenXmlFile(const Storage &storage, const Path &path, const std::string &password) {
    // TODO decryption

    // TODO open path from storage
    parent = std::make_unique<ZipReader>(path);

    generateMeta();
}

MicrosoftOpenXmlFile::MicrosoftOpenXmlFile(std::unique_ptr<Storage> parent) :
        parent(std::move(parent)) {
    generateMeta();
}

void MicrosoftOpenXmlFile::generateMeta() {
    static const std::unordered_map<Path, FileType> TYPES = {
            {"word/document.xml", FileType::OFFICE_OPEN_XML_DOCUMENT},
            {"ppt/presentation.xml", FileType::OFFICE_OPEN_XML_PRESENTATION},
            {"xl/workbook.xml", FileType::OFFICE_OPEN_XML_WORKBOOK},
    };

    meta = {};

    for (auto &&t : TYPES) {
        if (isFile(t.first)) {
            meta.type = t.second;
            break;
        }
    }
}

}
