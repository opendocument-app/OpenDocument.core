#include "OpenDocumentFile.h"
#include <cstdint>
#include <unordered_map>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/FileMeta.h"
#include "../MapUtil.h"
#include "../io/Path.h"
#include "../io/ZipStorage.h"
#include "../xml/XmlUtil.h"
#include "../crypto/CryptoUtil.h"

namespace odr {

namespace {
#ifdef ODR_CRYPTO
enum class ChecksumType {
    UNKNOWN, SHA256, SHA1, SHA256_1K, SHA1_1K
};
enum class AlgorithmType {
    UNKNOWN, AES256_CBC, TRIPLE_DES_CBC, BLOWFISH_CFB
};
enum class KeyDerivationType {
    UNKNOWN, PBKDF2
};
#endif

struct Entry {
    Path path;
    std::size_t sizeReal;
    std::size_t sizeUncompressed;
    bool encrypted;

#ifdef ODR_CRYPTO
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
#endif
};

bool lookupFileType(const std::string &mimetype, FileType &fileFype) {
    static const std::unordered_map<std::string, FileType> MIMETYPES = {
            {"application/vnd.oasis.opendocument.text", FileType::OPENDOCUMENT_TEXT},
            {"application/vnd.oasis.opendocument.presentation", FileType::OPENDOCUMENT_PRESENTATION},
            {"application/vnd.oasis.opendocument.spreadsheet", FileType::OPENDOCUMENT_SPREADSHEET},
            {"application/vnd.oasis.opendocument.graphics", FileType::OPENDOCUMENT_GRAPHICS},
    };
    return MapUtil::lookupMapDefault(MIMETYPES, mimetype, fileFype, FileType::UNKNOWN);
}
#ifdef ODR_CRYPTO
bool lookupChecksumType(const std::string &checksum, ChecksumType &checksumType) {
    static const std::unordered_map<std::string, ChecksumType> CHECKSUM_TYPES = {
            {"SHA1", ChecksumType::SHA1},
            {"SHA1/1K", ChecksumType::SHA1_1K},
            {"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0#sha256-1k", ChecksumType::SHA256_1K},
    };
    return MapUtil::lookupMapDefault(CHECKSUM_TYPES, checksum, checksumType, ChecksumType::UNKNOWN);
}
bool lookupAlgorithmTypes(const std::string &algorithm, AlgorithmType &algorithmType) {
    static const std::unordered_map<std::string, AlgorithmType> ALGORITHM_TYPES = {
            {"http://www.w3.org/2001/04/xmlenc#aes256-cbc", AlgorithmType::AES256_CBC},
            {"", AlgorithmType::TRIPLE_DES_CBC},
            {"Blowfish CFB", AlgorithmType::BLOWFISH_CFB},
    };
    return MapUtil::lookupMapDefault(ALGORITHM_TYPES, algorithm, algorithmType, AlgorithmType::UNKNOWN);
}
bool lookupKeyDerivationTypes(const std::string &keyDerivation, KeyDerivationType &keyDerivationType) {
    static const std::unordered_map<std::string, KeyDerivationType> KEY_DERIVATION_TYPES = {
            {"PBKDF2", KeyDerivationType::PBKDF2},
    };
    return MapUtil::lookupMapDefault(KEY_DERIVATION_TYPES, keyDerivation, keyDerivationType, KeyDerivationType::UNKNOWN);
}
bool lookupStartKeyTypes(const std::string &checksum, ChecksumType &checksumType) {
    static const std::unordered_map<std::string, ChecksumType> STARTKEY_TYPES = {
            {"SHA1", ChecksumType::SHA1},
            {"http://www.w3.org/2000/09/xmldsig#sha256", ChecksumType::SHA256},
    };
    return MapUtil::lookupMapDefault(STARTKEY_TYPES, checksum, checksumType, ChecksumType::UNKNOWN);
}
#endif
}

class OpenDocumentFile::Impl final {
public:
    std::unique_ptr<ZipReader> zip;
    FileMeta meta;
    std::unordered_map<Path, Entry> entries;
    std::string startKey;

    bool opened = false;
    bool decrypted = false;

    Entry *smallestEncryptedEntry = nullptr;

    void createEntries() {
        zip->visit([&](const auto &path) {
            if (zip->isFolder(path)) {
                return;
            }

            Entry entry{};
            entry.path = path;
            entry.sizeReal = zip->size(path);
            entry.sizeUncompressed = entry.sizeReal;
            entry.encrypted = false;
            entries[path] = entry;
        });
    }

    bool createMeta() {
        if (!isFile("content.xml")) {
            LOG(ERROR) << "content.xml not found";
            return false; // TODO throw
        }

        if (isFile("mimetype")) {
            const auto mimetype = loadEntry("mimetype");
            if (!lookupFileType(mimetype, meta.type)) {
                LOG(ERROR) << "mimetype not found: " << mimetype;
                return false; // TODO throw
            }
        }

        meta.encrypted = false;

        if (isFile("META-INF/manifest.xml")) {
            const auto manifest = loadXml("META-INF/manifest.xml");
            XmlUtil::recursiveVisitElementsWithName(manifest->RootElement(), "manifest:file-entry", [&](const tinyxml2::XMLElement &e) {
                const std::string fullPath = e.FindAttribute("manifest:full-path")->Value();

                if (fullPath == "/") {
                    const std::string mimetype = e.FindAttribute("manifest:media-type")->Value();
                    if (!lookupFileType(mimetype, meta.type)) {
                        LOG(ERROR) << "mimetype not found: " << mimetype;
                        return; // TODO throw
                    }
                    //meta.version = e->FindAttribute("manifest:version")->Value();
                    return;
                }

                const auto it = entries.find(fullPath);
                if (it == entries.end()) {
                    return;
                }

                Entry &entry = it->second;
                entry.path = fullPath;
                //entry.mediaType = e.FindAttribute("manifest:media-type")->Value();

                const tinyxml2::XMLElement *crypto = e.FirstChildElement("manifest:encryption-data");
                if (crypto == nullptr) {
                    return;
                }

                meta.encrypted = true;
                entry.encrypted = true;
                entry.sizeReal = e.FindAttribute("manifest:size")->UnsignedValue();

                if ((smallestEncryptedEntry == nullptr) || (entry.sizeUncompressed < smallestEncryptedEntry->sizeUncompressed)) {
                    smallestEncryptedEntry = &entry;
                }

#ifdef ODR_CRYPTO
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
                    if (key->FindAttribute("manifest:key-size") != nullptr) {
                        entry.keySize = key->FindAttribute("manifest:key-size")->UnsignedValue();
                    } else {
                        entry.keySize = 16;
                    }
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
#endif
            });
        }

        if (meta.type == FileType::UNKNOWN) {
            // TODO look into content.xml
        }

        return meta.type != FileType::UNKNOWN;
    }

    void createMeta2() {
        if (zip->isFile("meta.xml")) {
            const auto metaXml = loadXml("meta.xml");

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
        const auto contentXml = loadXml("content.xml");
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
                        // TODO: table dimension
                        meta.entries.emplace_back(entry);
                        tableHandle = tableHandle.NextSiblingElement("table:table");
                    }
                } break;
                default:
                    break;
            }
        }
    }

    bool open(const Path &path) {
        zip = std::make_unique<ZipReader>(path);

        createEntries();
        if (!createMeta()) {
            LOG(ERROR) << "could not get meta!";
            return false;
        }
        decrypted = !meta.encrypted;
        if (decrypted) {
            createMeta2();
        }

        opened = true;
        return true;
    }

    bool decrypt(const std::string &password) {
#ifdef ODR_CRYPTO
        if (!opened || decrypted) {
            return false;
        }
        if (smallestEncryptedEntry == nullptr) {
            LOG(ERROR) << "no smallest encrypted entry";
            return false;
        }
        if (!canDecrypt(*smallestEncryptedEntry)) {
            LOG(ERROR) << "cannot decrypt smallest encrypted entry";
            return false;
        }
        startKey = startKey_(*smallestEncryptedEntry, password);
        decrypted = validatePassword_();
        if (decrypted) createMeta2();
        return decrypted;
#else
        return false;
#endif
    }

#ifdef ODR_CRYPTO
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
                throw; // TODO
        }
    }

    static std::string decrypt_(const std::string &input, const std::string &derivedKey, const std::string &initialisationVector, const AlgorithmType algorithm) {
        switch (algorithm) {
            case AlgorithmType::AES256_CBC:
                return CryptoUtil::decryptAES(derivedKey, initialisationVector, input);
            case AlgorithmType::TRIPLE_DES_CBC:
                return CryptoUtil::decryptTripleDES(derivedKey, initialisationVector, input);
            case AlgorithmType::BLOWFISH_CFB:
                return CryptoUtil::decryptBlowfish(derivedKey, initialisationVector, input);
            default:
                throw;
        }
    }

    static std::string startKey_(const Entry &entry, const std::string &password) {
        const std::string result = hash_(password, entry.startKeyGeneration);
        if (result.size() < entry.startKeySize) {
            LOG(ERROR) << "start key too short";
            throw; // TODO
        }
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

    std::string deriveKeyAndDecrypt_(const Entry &entry, const std::string &input) {
        return deriveKeyAndDecrypt_(entry, startKey, input);
    }

    bool validatePassword_() {
        const Entry &entry = *smallestEncryptedEntry;
        const std::string decrypt = deriveKeyAndDecrypt_(entry, loadPlain(entry));
        return validatePassword_(entry, decrypt);
    }
#endif

    void close() {
        if (!opened) {
            return;
        }

        zip = nullptr;
        entries.clear();
        opened = false;
        decrypted = false;
        smallestEncryptedEntry = nullptr;
    }

    bool isFile(const Path &path) const {
        return zip->isFile(path);
    }

    std::string loadPlain(const Entry &entry) {
        const auto reader = zip->read(entry.path);
        std::string result(entry.sizeUncompressed, '\0');
        // TODO read until 0 return
        reader->read(&result[0], entry.sizeUncompressed);
        return result;
    }

    std::string loadEntry(const Path &path) {
        if (!zip->isFile(path)) {
            LOG(ERROR) << "zip entry size not found " << path;
            return ""; // TODO throw
        }
        const auto it = entries.find(path);
        if (it == entries.end()) {
            LOG(ERROR) << "zip entry not found " << path;
            return ""; // TODO throw
        }
        std::string result = loadPlain(it->second);
        if (result.size() != it->second.sizeUncompressed) {
            LOG(ERROR) << "zip entry size doesn't match " << path;
            return ""; // TODO throw
        }
#ifdef ODR_CRYPTO
        if (it->second.encrypted) {
            if (!decrypted) {
                LOG(ERROR) << "not decrypted";
                return ""; // TODO throw
            }
            if (!canDecrypt(it->second)) {
                LOG(ERROR) << "cannot decrypt " << path;
                return ""; // TODO throw
            }
            result = CryptoUtil::inflate(deriveKeyAndDecrypt_(it->second, result));
            if (result.size() != it->second.sizeReal) {
                LOG(ERROR) << "inflated size doesn't match " << path;
                return ""; // TODO throw
            }
        }
#endif
        return result;
    }

    std::unique_ptr<tinyxml2::XMLDocument> loadXml(const Path &path) {
        if (!zip->isFile(path)) {
            return nullptr;
        }
        const auto xml = loadEntry(path);
        auto result = std::make_unique<tinyxml2::XMLDocument>();
        tinyxml2::XMLError error = result->Parse(xml.data(), xml.size());
        if (error != tinyxml2::XML_SUCCESS) {
            return nullptr; // TODO throw
        }
        return result;
    }
};

OpenDocumentFile::OpenDocumentFile() :
        impl(std::make_unique<Impl>()) {
}

OpenDocumentFile::~OpenDocumentFile() = default;

bool OpenDocumentFile::open(const Path &path) {
    return impl->open(path);
}

bool OpenDocumentFile::decrypt(const std::string &password) {
    return impl->decrypt(password);
}

void OpenDocumentFile::close() {
    return impl->close();
}

bool OpenDocumentFile::isOpen() const {
    return impl->opened;
}

bool OpenDocumentFile::isDecrypted() const {
    return impl->decrypted;
}

const FileMeta &OpenDocumentFile::getMeta() const {
    return impl->meta;
}

bool OpenDocumentFile::isFile(const Path &path) const {
    return impl->isFile(path);
}

std::string OpenDocumentFile::loadEntry(const Path &path) {
    return impl->loadEntry(path);
}

std::unique_ptr<tinyxml2::XMLDocument> OpenDocumentFile::loadXml(const Path &path) {
    return impl->loadXml(path);
}

const ZipReader &OpenDocumentFile::getZipReader() const {
    return *impl->zip;
}

}
