#include "OpenDocumentMeta.h"
#include <unordered_map>
#include "tinyxml2.h"
#include "odr/FileMeta.h"
#include "../TableCursor.h"
#include "../MapUtil.h"
#include "../XmlUtil.h"
#include "../io/Storage.h"
#include "../io/StorageUtil.h"
#include "../crypto/CryptoUtil.h"

namespace odr {

static bool lookupFileType(const std::string &mimeType, FileType &fileType) {
    static const std::unordered_map<std::string, FileType> MIME_TYPES = {
            {"application/vnd.oasis.opendocument.text", FileType::OPENDOCUMENT_TEXT},
            {"application/vnd.oasis.opendocument.presentation", FileType::OPENDOCUMENT_PRESENTATION},
            {"application/vnd.oasis.opendocument.spreadsheet", FileType::OPENDOCUMENT_SPREADSHEET},
            {"application/vnd.oasis.opendocument.graphics", FileType::OPENDOCUMENT_GRAPHICS},
    };
    return MapUtil::lookupMapDefault(MIME_TYPES, mimeType, fileType, FileType::UNKNOWN);
}

static bool lookupChecksumType(const std::string &checksum, OpenDocumentMeta::ChecksumType &checksumType) {
    static const std::unordered_map<std::string, OpenDocumentMeta::ChecksumType> CHECKSUM_TYPES = {
            {"SHA1", OpenDocumentMeta::ChecksumType::SHA1},
            {"SHA1/1K", OpenDocumentMeta::ChecksumType::SHA1_1K},
            {"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0#sha256-1k", OpenDocumentMeta::ChecksumType::SHA256_1K},
    };
    return MapUtil::lookupMapDefault(CHECKSUM_TYPES, checksum, checksumType, OpenDocumentMeta::ChecksumType::UNKNOWN);
}

static bool lookupAlgorithmTypes(const std::string &algorithm, OpenDocumentMeta::AlgorithmType &algorithmType) {
    static const std::unordered_map<std::string, OpenDocumentMeta::AlgorithmType> ALGORITHM_TYPES = {
            {"http://www.w3.org/2001/04/xmlenc#aes256-cbc", OpenDocumentMeta::AlgorithmType::AES256_CBC},
            {"", OpenDocumentMeta::AlgorithmType::TRIPLE_DES_CBC},
            {"Blowfish CFB", OpenDocumentMeta::AlgorithmType::BLOWFISH_CFB},
    };
    return MapUtil::lookupMapDefault(ALGORITHM_TYPES, algorithm, algorithmType, OpenDocumentMeta::AlgorithmType::UNKNOWN);
}

static bool lookupKeyDerivationTypes(const std::string &keyDerivation, OpenDocumentMeta::KeyDerivationType &keyDerivationType) {
    static const std::unordered_map<std::string, OpenDocumentMeta::KeyDerivationType> KEY_DERIVATION_TYPES = {
            {"PBKDF2", OpenDocumentMeta::KeyDerivationType::PBKDF2},
    };
    return MapUtil::lookupMapDefault(KEY_DERIVATION_TYPES, keyDerivation, keyDerivationType, OpenDocumentMeta::KeyDerivationType::UNKNOWN);
}

static bool lookupStartKeyTypes(const std::string &checksum, OpenDocumentMeta::ChecksumType &checksumType) {
    static const std::unordered_map<std::string, OpenDocumentMeta::ChecksumType> STARTKEY_TYPES = {
            {"SHA1", OpenDocumentMeta::ChecksumType::SHA1},
            {"http://www.w3.org/2000/09/xmldsig#sha256", OpenDocumentMeta::ChecksumType::SHA256},
    };
    return MapUtil::lookupMapDefault(STARTKEY_TYPES, checksum, checksumType, OpenDocumentMeta::ChecksumType::UNKNOWN);
}

static void estimateTableDimensions(const tinyxml2::XMLElement &table, std::uint32_t &rows, std::uint32_t &cols,
        const std::uint32_t limitRows, const std::uint32_t limitCols) {
    rows = 0;
    cols = 0;

    TableCursor tl;

    // TODO we dont need to recurse so deep
    XmlUtil::recursiveVisitElements(&table, [&](const auto &e) {
        if (e.Name() == std::string("table:table-row")) {
            const auto repeated = e.Unsigned64Attribute("table:number-rows-repeated", 1);
            tl.addRow(repeated);
        } else if (e.Name() == std::string("table:table-cell")) {
            const auto repeated = e.Unsigned64Attribute("table:number-columns-repeated", 1);
            const auto colspan = e.Unsigned64Attribute("table:number-columns-spanned", 1);
            const auto rowspan = e.Unsigned64Attribute("table:number-rows-spanned", 1);
            tl.addCell(colspan, rowspan, repeated);

            const auto newRows = tl.getRow();
            const auto newCols = std::max(cols, tl.getCol());
            if ((e.FirstChild() != nullptr) &&
                (((limitRows != 0) && (newRows < limitRows)) && ((limitCols != 0) && (newCols < limitCols)))) {
                rows = newRows;
                cols = newCols;
            }
        }
    });
}

FileMeta OpenDocumentMeta::parseFileMeta(Storage &storage, const bool decrypted) {
    FileMeta result{};

    if (!storage.isFile("content.xml")) throw NoOpenDocumentFileException();

    if (storage.isFile("mimetype")) {
        const auto mimeType = StorageUtil::read(storage, "mimetype");
        lookupFileType(mimeType, result.type);
    }

    if (storage.isFile("META-INF/manifest.xml")) {
        const auto manifest = XmlUtil::parse(storage, "META-INF/manifest.xml");
        XmlUtil::recursiveVisitElementsWithName(manifest->RootElement(), "manifest:file-entry", [&](const tinyxml2::XMLElement &e) {
            const Path path = e.FindAttribute("manifest:full-path")->Value();
            if (path == "/" && e.FindAttribute("manifest:media-type") != nullptr) {
                const std::string mimeType = e.FindAttribute("manifest:media-type")->Value();
                lookupFileType(mimeType, result.type);
            }
        });
        XmlUtil::recursiveVisitElementsWithName(manifest->RootElement(), "manifest:encryption-data", [&](const tinyxml2::XMLElement &) {
            result.encrypted = true;
        });
    }

    if (result.encrypted == decrypted) {
        if (storage.isFile("meta.xml")) {
            const auto metaXml = XmlUtil::parse(storage, "meta.xml");

            tinyxml2::XMLElement *statisticsElement = tinyxml2::XMLHandle(metaXml.get())
                    .FirstChildElement("office:document-meta")
                    .FirstChildElement("office:meta")
                    .FirstChildElement("meta:document-statistic")
                    .ToElement();
            if (statisticsElement != nullptr) {
                switch (result.type) {
                    case FileType::OPENDOCUMENT_TEXT: {
                        const tinyxml2::XMLAttribute *pageCount = statisticsElement->FindAttribute("meta:page-count");
                        if (pageCount == nullptr) {
                            break;
                        }
                        result.entryCount = pageCount->UnsignedValue();
                    } break;
                    case FileType::OPENDOCUMENT_PRESENTATION: {
                        result.entryCount = 0;
                    } break;
                    case FileType::OPENDOCUMENT_SPREADSHEET: {
                        const tinyxml2::XMLAttribute *tableCount = statisticsElement->FindAttribute("meta:table-count");
                        if (tableCount == nullptr) {
                            break;
                        }
                        result.entryCount = tableCount->UnsignedValue();
                    } break;
                    case FileType::OPENDOCUMENT_GRAPHICS: {
                    } break;
                    default:
                        break;
                }
            }
        }

        // TODO: dont load content twice (happens in case of translation)
        const auto contentXml = XmlUtil::parse(storage, "content.xml");
        tinyxml2::XMLHandle bodyHandle = tinyxml2::XMLHandle(contentXml.get())
                .FirstChildElement("office:document-content")
                .FirstChildElement("office:body");
        if (bodyHandle.ToElement() == nullptr) throw NoOpenDocumentFileException();

        switch (result.type) {
            case FileType::OPENDOCUMENT_PRESENTATION: {
                result.entryCount = 0;
                XmlUtil::recursiveVisitElementsWithName(bodyHandle.ToElement(), "draw:page", [&](const tinyxml2::XMLElement &e) {
                    ++result.entryCount;
                    FileMeta::Entry entry;
                    entry.name = e.FindAttribute("draw:name")->Value();
                    result.entries.emplace_back(entry);
                });
            } break;
            case FileType::OPENDOCUMENT_SPREADSHEET: {
                result.entryCount = 0;
                XmlUtil::recursiveVisitElementsWithName(bodyHandle.ToElement(), "table:table", [&](const tinyxml2::XMLElement &e) {
                    ++result.entryCount;
                    FileMeta::Entry entry;
                    entry.name = e.FindAttribute("table:name")->Value();
                    // TODO configuration
                    estimateTableDimensions(e, entry.rowCount, entry.columnCount, 10000, 500);
                    result.entries.emplace_back(entry);
                });
            } break;
            default:
                break;
        }
    }

    return result;
}

OpenDocumentMeta::Manifest OpenDocumentMeta::parseManifest(Storage &storage) {
    if (!storage.isFile("META-INF/manifest.xml")) throw NoOpenDocumentFileException();
    const auto manifest = XmlUtil::parse(storage, "META-INF/manifest.xml");
    return parseManifest(*manifest);
}

OpenDocumentMeta::Manifest OpenDocumentMeta::parseManifest(const tinyxml2::XMLDocument &manifest) {
    Manifest result{};

    XmlUtil::recursiveVisitElementsWithName(manifest.RootElement(), "manifest:file-entry", [&](const tinyxml2::XMLElement &e) {
        const Path path = e.FindAttribute("manifest:full-path")->Value();
        const tinyxml2::XMLElement *crypto = e.FirstChildElement("manifest:encryption-data");
        if (crypto == nullptr) return;
        result.encryted = true;

        Manifest::Entry entry{};
        entry.size = e.FindAttribute("manifest:size")->UnsignedValue();

        { // checksum
            const std::string checksumType = crypto->FindAttribute("manifest:checksum-type")->Value();
            lookupChecksumType(checksumType, entry.checksumType);
            entry.checksum = crypto->FindAttribute("manifest:checksum")->Value();
        }

        { // encryption algorithm
            const tinyxml2::XMLElement *algorithm = crypto->FirstChildElement("manifest:algorithm");
            const std::string algorithmName = algorithm->FindAttribute("manifest:algorithm-name")->Value();
            lookupAlgorithmTypes(algorithmName, entry.algorithm);
            entry.initialisationVector = algorithm->FindAttribute("manifest:initialisation-vector")->Value();
        }

        { // key derivation
            const tinyxml2::XMLElement *key = crypto->FirstChildElement("manifest:key-derivation");
            const std::string keyDerivationName = key->FindAttribute("manifest:key-derivation-name")->Value();
            lookupKeyDerivationTypes(keyDerivationName, entry.keyDerivation);
            entry.keySize = key->Unsigned64Attribute("manifest:key-size", 16);
            entry.keyIterationCount = key->FindAttribute("manifest:iteration-count")->UnsignedValue();
            entry.keySalt = key->FindAttribute("manifest:salt")->Value();
        }

        { // start key generation
            const tinyxml2::XMLElement *start = crypto->FirstChildElement("manifest:start-key-generation");
            if (start != nullptr) {
                const std::string startKeyGenerationName = start->FindAttribute("manifest:start-key-generation-name")->Value();
                lookupStartKeyTypes(startKeyGenerationName, entry.startKeyGeneration);
                entry.startKeySize = start->FindAttribute("manifest:key-size")->UnsignedValue();
            } else {
                entry.startKeyGeneration = ChecksumType::SHA1;
                entry.startKeySize = 20;
            }
        }

        entry.checksum = CryptoUtil::base64Decode(entry.checksum);
        entry.initialisationVector = CryptoUtil::base64Decode(entry.initialisationVector);
        entry.keySalt = CryptoUtil::base64Decode(entry.keySalt);

        const auto it = result.entries.emplace(path, entry).first;
        if ((result.smallestFilePath == nullptr) || (entry.size < result.smallestFileSize)) {
            result.smallestFileSize = entry.size;
            result.smallestFilePath = &it->first;
            result.smallestFileEntry = &it->second;
        }
    });

    return result;
}

}
