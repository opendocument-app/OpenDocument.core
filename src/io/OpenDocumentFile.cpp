#include "OpenDocumentFile.h"
#include <cstdint>
#include <unordered_map>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/FileMeta.h"
#include "Path.h"
#include "StorageUtil.h"
#include "ZipStorage.h"
#include "../TableLocation.h"
#include "../MapUtil.h"
#include "../xml/XmlUtil.h"
#include "../crypto/CryptoUtil.h"

namespace odr {

#ifdef ODR_CRYPTO
namespace {
class CryptoOpenDocumentFile : public ReadStorage {
public:
    enum class ChecksumType {
        UNKNOWN, SHA256, SHA1, SHA256_1K, SHA1_1K
    };
    enum class AlgorithmType {
        UNKNOWN, AES256_CBC, TRIPLE_DES_CBC, BLOWFISH_CFB
    };
    enum class KeyDerivationType {
        UNKNOWN, PBKDF2
    };

    struct Entry {
        std::size_t size;

        ChecksumType checksumType = ChecksumType::UNKNOWN;
        std::string checksum;
        AlgorithmType algorithm = AlgorithmType::UNKNOWN;
        std::string initialisationVector;
        KeyDerivationType keyDerivation = KeyDerivationType::UNKNOWN;
        std::uint64_t keySize;
        std::uint64_t keyIterationCount;
        std::string keySalt;
        ChecksumType startKeyGeneration = ChecksumType::UNKNOWN;
        std::uint64_t startKeySize;
    };

    static bool lookupChecksumType(const std::string &checksum, ChecksumType &checksumType) {
        static const std::unordered_map<std::string, ChecksumType> CHECKSUM_TYPES = {
                {"SHA1", ChecksumType::SHA1},
                {"SHA1/1K", ChecksumType::SHA1_1K},
                {"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0#sha256-1k", ChecksumType::SHA256_1K},
        };
        return MapUtil::lookupMapDefault(CHECKSUM_TYPES, checksum, checksumType, ChecksumType::UNKNOWN);
    }
    static bool lookupAlgorithmTypes(const std::string &algorithm, AlgorithmType &algorithmType) {
        static const std::unordered_map<std::string, AlgorithmType> ALGORITHM_TYPES = {
                {"http://www.w3.org/2001/04/xmlenc#aes256-cbc", AlgorithmType::AES256_CBC},
                {"", AlgorithmType::TRIPLE_DES_CBC},
                {"Blowfish CFB", AlgorithmType::BLOWFISH_CFB},
        };
        return MapUtil::lookupMapDefault(ALGORITHM_TYPES, algorithm, algorithmType, AlgorithmType::UNKNOWN);
    }
    static bool lookupKeyDerivationTypes(const std::string &keyDerivation, KeyDerivationType &keyDerivationType) {
        static const std::unordered_map<std::string, KeyDerivationType> KEY_DERIVATION_TYPES = {
                {"PBKDF2", KeyDerivationType::PBKDF2},
        };
        return MapUtil::lookupMapDefault(KEY_DERIVATION_TYPES, keyDerivation, keyDerivationType, KeyDerivationType::UNKNOWN);
    }
    static bool lookupStartKeyTypes(const std::string &checksum, ChecksumType &checksumType) {
        static const std::unordered_map<std::string, ChecksumType> STARTKEY_TYPES = {
                {"SHA1", ChecksumType::SHA1},
                {"http://www.w3.org/2000/09/xmldsig#sha256", ChecksumType::SHA256},
        };
        return MapUtil::lookupMapDefault(STARTKEY_TYPES, checksum, checksumType, ChecksumType::UNKNOWN);
    }

    static bool canDecrypt(const Entry &entry) {
        return (entry.checksumType != ChecksumType::UNKNOWN)
               && (entry.algorithm != AlgorithmType::UNKNOWN)
               && (entry.keyDerivation != KeyDerivationType::UNKNOWN)
               && (entry.startKeyGeneration != ChecksumType::UNKNOWN);
    }

    static std::string hash_(const std::string &input, const ChecksumType checksumType) {
        switch (checksumType) {
            case ChecksumType::SHA256:
                return CryptoUtil::sha256(input);
            case ChecksumType::SHA1:
                return CryptoUtil::sha1(input);
            case ChecksumType::SHA256_1K:
                return CryptoUtil::sha256(input.substr(0, 1024));
            case ChecksumType::SHA1_1K:
                return CryptoUtil::sha1(input.substr(0, 1024));
            default:
                throw std::invalid_argument("checksumType");
        }
    }

    static std::string decrypt_(const std::string &input, const std::string &derivedKey,
            const std::string &initialisationVector, const AlgorithmType algorithm) {
        switch (algorithm) {
            case AlgorithmType::AES256_CBC:
                return CryptoUtil::decryptAES(derivedKey, initialisationVector, input);
            case AlgorithmType::TRIPLE_DES_CBC:
                return CryptoUtil::decryptTripleDES(derivedKey, initialisationVector, input);
            case AlgorithmType::BLOWFISH_CFB:
                return CryptoUtil::decryptBlowfish(derivedKey, initialisationVector, input);
            default:
                throw std::invalid_argument("algorithm");
        }
    }

    static std::string startKey_(const Entry &entry, const std::string &password) {
        const std::string result = hash_(password, entry.startKeyGeneration);
        if (result.size() < entry.startKeySize) throw std::invalid_argument("password");
        return result.substr(0, entry.startKeySize);
    }

    static std::string deriveKeyAndDecrypt_(const Entry &entry, const std::string &startKey, const std::string &input) {
        const std::string derivedKey = CryptoUtil::pbkdf2(entry.keySize, startKey, entry.keySalt, entry.keyIterationCount);
        return decrypt_(input, derivedKey, entry.initialisationVector, entry.algorithm);
    }

    static bool validatePassword_(const Entry &entry, std::string decrypted) {
        const std::size_t padding = CryptoUtil::padding(decrypted);
        decrypted = decrypted.substr(0, decrypted.size() - padding);
        const std::string checksum = hash_(decrypted, entry.checksumType);
        return checksum == entry.checksum;
    }

    std::unique_ptr<Storage> parent;

    bool encryted;
    std::unordered_map<Path, Entry> entries;

    std::uint64_t smallestFileSize;
    const Path *smallestFilePath;
    const Entry *smallestFileEntry;

    std::string startKey;

    CryptoOpenDocumentFile(std::unique_ptr<Storage> parent, const std::string &password) :
            parent(std::move(parent)) {
        parseManifest();
        decrypt(password);
    }

    void parseManifest() {
        if (!isFile("META-INF/manifest.xml")) {
            return; // TODO throw
        }

        encryted = false;
        smallestFileSize = 0;
        smallestFilePath = nullptr;
        smallestFileEntry = nullptr;

        const auto manifest = XmlUtil::parse(*this, "META-INF/manifest.xml");
        XmlUtil::recursiveVisitElementsWithName(manifest->RootElement(), "manifest:file-entry", [&](const tinyxml2::XMLElement &e) {
            const Path path = e.FindAttribute("manifest:full-path")->Value();
            if (!parent->isFile(path)) return;

            const tinyxml2::XMLElement *crypto = e.FirstChildElement("manifest:encryption-data");
            if (crypto == nullptr) return;

            encryted = true;

            Entry entry{};
            entry.size = e.FindAttribute("manifest:size")->UnsignedValue();

            { // checksum
                const std::string checksumType = crypto->FindAttribute("manifest:checksum-type")->Value();
                if (!lookupChecksumType(checksumType, entry.checksumType)) {
                    LOG(ERROR) << "unknown checksum " << checksumType;
                    // TODO throw
                }
                entry.checksum = crypto->FindAttribute("manifest:checksum")->Value();
            }

            { // encryption algorithm
                const tinyxml2::XMLElement *algorithm = crypto->FirstChildElement("manifest:algorithm");
                const std::string algorithmName = algorithm->FindAttribute("manifest:algorithm-name")->Value();
                if (!lookupAlgorithmTypes(algorithmName, entry.algorithm)) {
                    LOG(ERROR) << "unknown algorithm " << algorithmName;
                    // TODO throw
                }
                entry.initialisationVector = algorithm->FindAttribute("manifest:initialisation-vector")->Value();
            }

            { // key derivation
                const tinyxml2::XMLElement *key = crypto->FirstChildElement("manifest:key-derivation");
                const std::string keyDerivationName = key->FindAttribute("manifest:key-derivation-name")->Value();
                if (!lookupKeyDerivationTypes(keyDerivationName, entry.keyDerivation)) {
                    LOG(ERROR) << "unknown key derivation " << keyDerivationName;
                    // TODO throw
                }
                entry.keySize = key->Unsigned64Attribute("manifest:key-size", 16);
                entry.keyIterationCount = key->FindAttribute("manifest:iteration-count")->UnsignedValue();
                entry.keySalt = key->FindAttribute("manifest:salt")->Value();
            }

            { // start key generation
                const tinyxml2::XMLElement *start = crypto->FirstChildElement("manifest:start-key-generation");
                if (start != nullptr) {
                    const std::string startKeyGenerationName = start->FindAttribute("manifest:start-key-generation-name")->Value();
                    if (!lookupStartKeyTypes(startKeyGenerationName, entry.startKeyGeneration)) {
                        LOG(ERROR) << "unknown start key generation " << startKeyGenerationName;
                        // TODO throw
                    }
                    entry.startKeySize = start->FindAttribute("manifest:key-size")->UnsignedValue();
                } else {
                    entry.startKeyGeneration = ChecksumType::SHA1;
                    entry.startKeySize = 20;
                }
            }

            entry.checksum = CryptoUtil::base64Decode(entry.checksum);
            entry.initialisationVector = CryptoUtil::base64Decode(entry.initialisationVector);
            entry.keySalt = CryptoUtil::base64Decode(entry.keySalt);

            const auto it = entries.emplace(path, entry).first;
            if ((smallestFilePath == nullptr) || (entry.size < smallestFileSize)) {
                smallestFileSize = entry.size;
                smallestFilePath = &it->first;
                smallestFileEntry = &it->second;
            }
        });

        if (smallestFileEntry == nullptr) throw FileNotFoundException("smallestFileEntry");
    }

    void decrypt(const std::string &password) {
        if (!encryted) return;
        if (!canDecrypt(*smallestFileEntry)) throw UnsupportedCryptoAlgorithmException();
        startKey = startKey_(*smallestFileEntry, password);
        const std::string input = StorageUtil::read(*this, *smallestFilePath);
        const std::string decrypt = deriveKeyAndDecrypt_(*smallestFileEntry, startKey, input);
        const bool result = validatePassword_(*smallestFileEntry, decrypt);
        if (!result) throw WrongPasswordException();
    }

    bool isSomething(const Path &p) const final { return parent->isSomething(p); }
    bool isFile(const Path &p) const final { return parent->isSomething(p); }
    bool isFolder(const Path &p) const final { return parent->isSomething(p); }
    bool isReadable(const Path &p) const final { return isFile(p); }

    std::uint64_t size(const Path &p) const final {
        const auto it = entries.find(p);
        if (it == entries.end()) return parent->size(p);
        return it->second.size;
    }

    void visit(const Path &p, Visiter v) const final { parent->visit(p, v); }

    std::unique_ptr<Source> read(const Path &p) const final {
        const auto it = entries.find(p);
        if (it == entries.end()) return parent->read(p);
        if (!canDecrypt(it->second)) throw UnsupportedCryptoAlgorithmException();
        //result = CryptoUtil::inflate(deriveKeyAndDecrypt_(it->second, result));
        return nullptr; // TODO
    }
};
}
#endif

static bool lookupFileType(const std::string &mimeType, FileType &fileType) {
    static const std::unordered_map<std::string, FileType> MIME_TYPES = {
            {"application/vnd.oasis.opendocument.text", FileType::OPENDOCUMENT_TEXT},
            {"application/vnd.oasis.opendocument.presentation", FileType::OPENDOCUMENT_PRESENTATION},
            {"application/vnd.oasis.opendocument.spreadsheet", FileType::OPENDOCUMENT_SPREADSHEET},
            {"application/vnd.oasis.opendocument.graphics", FileType::OPENDOCUMENT_GRAPHICS},
    };
    return MapUtil::lookupMapDefault(MIME_TYPES, mimeType, fileType, FileType::UNKNOWN);
}

static void estimateTableDimensions(const tinyxml2::XMLElement &table, std::uint32_t &rows, std::uint32_t &cols,
        const std::uint32_t limitRows, const std::uint32_t limitCols) {
    rows = 0;
    cols = 0;

    TableLocation tl;

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

            const auto newRows = tl.getNextRow();
            const auto newCols = std::max(cols, tl.getNextCol());
            if ((e.FirstChild() != nullptr) &&
                (((limitRows != 0) && (newRows < limitRows)) && ((limitCols != 0) && (newCols < limitCols)))) {
                rows = newRows;
                cols = newCols;
            }
        }
    });
}

OpenDocumentFile::OpenDocumentFile(const Path &path, const std::string &password) {
    parent = std::make_unique<ZipReader>(path);
#ifdef ODR_CRYPTO
    // TODO only if encrypted
    parent = std::make_unique<CryptoOpenDocumentFile>(std::move(parent), password);
#endif

    parseMeta();
}

OpenDocumentFile::OpenDocumentFile(const Storage &, const Path &path, const std::string &password) {
    // TODO open from storage
    parent = std::make_unique<ZipReader>(path);
#ifdef ODR_CRYPTO
    // TODO only if encrypted
    parent = std::make_unique<CryptoOpenDocumentFile>(std::move(parent), password);
#endif

    parseMeta();
}

OpenDocumentFile::OpenDocumentFile(std::unique_ptr<Storage> parent) :
        parent(std::move(parent)) {
    parseMeta();
}

// TODO out-source meta parsing
void OpenDocumentFile::parseMeta() {
    if (!isFile("content.xml")) {
        LOG(ERROR) << "content.xml not found";
        return; // TODO throw
    }

    if (isFile("mimetype")) {
        const auto mimetype = StorageUtil::read(*this, "mimetype");
        if (!lookupFileType(mimetype, meta.type)) {
            LOG(ERROR) << "mimetype not found: " << mimetype;
            return; // TODO throw
        }
    }

    if (parent->isFile("meta.xml")) {
        const auto metaXml = XmlUtil::parse(*this, "meta.xml");

        tinyxml2::XMLElement *statisticsElement = tinyxml2::XMLHandle(metaXml.get())
                .FirstChildElement("office:document-meta")
                .FirstChildElement("office:meta")
                .FirstChildElement("meta:document-statistic")
                .ToElement();
        if (statisticsElement != nullptr) {
            switch (meta.type) {
                case FileType::OPENDOCUMENT_TEXT: {
                    const tinyxml2::XMLAttribute *pageCount = statisticsElement->FindAttribute("meta:page-count");
                    if (pageCount == nullptr) {
                        break;
                    }
                    meta.entryCount = pageCount->UnsignedValue();
                } break;
                case FileType::OPENDOCUMENT_PRESENTATION: {
                    meta.entryCount = 0;
                } break;
                case FileType::OPENDOCUMENT_SPREADSHEET: {
                    const tinyxml2::XMLAttribute *tableCount = statisticsElement->FindAttribute("meta:table-count");
                    if (tableCount == nullptr) {
                        break;
                    }
                    meta.entryCount = tableCount->UnsignedValue();
                } break;
                case FileType::OPENDOCUMENT_GRAPHICS: {
                } break;
                default:
                    break;
            }
        }
    }

    // TODO: dont load content twice (happens in case of translation)
    const auto contentXml = XmlUtil::parse(*this, "content.xml");
    tinyxml2::XMLHandle bodyHandle = tinyxml2::XMLHandle(contentXml.get())
            .FirstChildElement("office:document-content")
            .FirstChildElement("office:body");
    if (bodyHandle.ToElement() != nullptr) {
        switch (meta.type) {
            case FileType::OPENDOCUMENT_PRESENTATION: {
                meta.entryCount = 0;
                tinyxml2::XMLHandle pageHandle = bodyHandle
                        .FirstChildElement("office:presentation")
                        .FirstChildElement("draw:page");
                while (pageHandle.ToElement() != nullptr) {
                    ++meta.entryCount;
                    FileMeta::Entry entry;
                    entry.name = pageHandle.ToElement()->FindAttribute("draw:name")->Value();
                    meta.entries.emplace_back(entry);
                    pageHandle = pageHandle.NextSiblingElement("draw:page");
                }
            } break;
            case FileType::OPENDOCUMENT_SPREADSHEET: {
                meta.entryCount = 0;
                tinyxml2::XMLHandle tableHandle = bodyHandle
                        .FirstChildElement("office:spreadsheet")
                        .FirstChildElement("table:table");
                while (tableHandle.ToElement() != nullptr) {
                    ++meta.entryCount;
                    FileMeta::Entry entry;
                    entry.name = tableHandle.ToElement()->FindAttribute("table:name")->Value();
                    // TODO configuration
                    estimateTableDimensions(*tableHandle.ToElement(), entry.rowCount, entry.columnCount, 10000, 500);
                    meta.entries.emplace_back(entry);
                    tableHandle = tableHandle.NextSiblingElement("table:table");
                }
            } break;
            default:
                break;
        }
    }
}

}
