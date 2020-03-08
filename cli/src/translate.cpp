#include <odr/Config.h>
#include <odr/Meta.h>
#include <odr/OpenDocumentReader.h>
#include <iostream>
#include <string>

static void print_meta(const odr::OpenDocumentReader &odr) {
    const auto &meta = odr.getMeta();

    std::cout << "type " << (int) meta.type << " ";
    switch (meta.type) {
        case odr::FileType::ZIP: std::cout << "zip"; break;
        case odr::FileType::COMPOUND_FILE_BINARY_FORMAT: std::cout << "cfb"; break;
        case odr::FileType::OPENDOCUMENT_TEXT: std::cout << "odr"; break;
        case odr::FileType::OPENDOCUMENT_SPREADSHEET: std::cout << "ods"; break;
        case odr::FileType::OPENDOCUMENT_PRESENTATION: std::cout << "odp"; break;
        case odr::FileType::OPENDOCUMENT_GRAPHICS: std::cout << "odg"; break;
        case odr::FileType::OFFICE_OPEN_XML_DOCUMENT: std::cout << "docx"; break;
        case odr::FileType::OFFICE_OPEN_XML_WORKBOOK: std::cout << "xlsx"; break;
        case odr::FileType::OFFICE_OPEN_XML_PRESENTATION: std::cout << "pptx"; break;
        default: std::cout << "unnamed"; break;
    }

    if (!meta.entries.empty()) {
        std::cout << " entries " << meta.entryCount << " ";
        for (auto &&e : meta.entries) {
            std::cout << "\"" << e.name << "\" ";
        }
    }

    std::cout << std::endl;
}

int main(int argc, char **argv) {
    const std::string input(argv[1]);
    const std::string output(argv[2]);

    bool hasPassword = argc >= 4;
    std::string password;
    if (hasPassword) password = argv[3];

    odr::Config config = {};
    config.entryOffset = 0;
    config.entryCount = 0;
    config.editable = true;

    bool success;

    odr::OpenDocumentReader odr;
    success = odr.open(input);
    if (!success) return 1;

    print_meta(odr);
    if (hasPassword) {
        success = odr.decrypt(password);
        if (!success) return 2;
        print_meta(odr);
    }

    success = odr.translate(output, config);
    if (!success) return 3;

    return 0;
}
