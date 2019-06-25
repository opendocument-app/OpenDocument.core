#include "OpenDocumentFile.h"
#include <fstream>
#include <algorithm>
#include "miniz_zip.h"
#include "tinyxml2.h"
#include "glog/logging.h"
#include "CryptoUtil.h"

namespace odr {

typedef OpenDocumentFile::Entries Entries;

static const std::map<std::string, FileType> MIMETYPES = {
        {"application/vnd.oasis.opendocument.text", FileType::OPENDOCUMENT_TEXT},
        {"application/vnd.oasis.opendocument.presentation", FileType::OPENDOCUMENT_PRESENTATION},
        {"application/vnd.oasis.opendocument.spreadsheet", FileType::OPENDOCUMENT_SPREADSHEET},
        {"application/vnd.oasis.opendocument.graphics", FileType::OPENDOCUMENT_GRAPHICS},
};

#ifdef ODR_CRYPTO
static const std::map<std::string, ChecksumType> CHECKSUM_TYPES = {
        {"SHA1", ChecksumType::SHA1},
        {"SHA1/1K", ChecksumType::SHA1_1K},
        {"urn:oasis:names:tc:opendocument:xmlns:manifest:1.0#sha256-1k", ChecksumType::SHA256_1K},
};
static const std::map<std::string, AlgorithmType> ALGORITHM_TYPES = {
        {"http://www.w3.org/2001/04/xmlenc#aes256-cbc", AlgorithmType::AES256_CBC},
        {"", AlgorithmType::TRIPLE_DES_CBC},
        {"Blowfish CFB", AlgorithmType::BLOWFISH_CFB},
};
static const std::map<std::string, KeyDerivationType> KEY_DERIVATION_TYPES = {
        {"PBKDF2", KeyDerivationType::PBKDF2},
};
static const std::map<std::string, ChecksumType> STARTKEY_TYPES = {
        {"SHA1", ChecksumType::SHA1},
        {"http://www.w3.org/2000/09/xmldsig#sha256", ChecksumType::SHA256},
};
#endif

class OpenDocumentFileImpl {
public:
    mz_zip_archive zip;
    FileMeta meta;
    Entries entries;
    std::string startKey;

    bool opened = false;
    bool decrypted = false;

    OpenDocumentEntry *smallestEncryptedEntry = nullptr;

    ~OpenDocumentFileImpl() {
        close();
    }

    bool createEntries() {
        for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip); ++i) {
            mz_zip_archive_file_stat entry_stat;
            if (!mz_zip_reader_file_stat(&zip, i, &entry_stat)) {
                mz_zip_reader_end(&zip);
                return false;
            }

            if (mz_zip_reader_is_file_a_directory(&zip, i)) {
                continue;
            }

            OpenDocumentEntry entry;
            entry.size_real = entry_stat.m_uncomp_size;
            entry.size_uncompressed = entry_stat.m_uncomp_size;
            entry.size_compressed = entry_stat.m_comp_size;
            entry.index = i;
            entry.encrypted = false;
            entries[entry_stat.m_filename] = entry;
        }

        return true;
    }

    bool createMeta() {
        if (!isFile("content.xml") | !isFile("styles.xml")) {
            return false;
        }

        if (isFile("mimetype")) {
            auto mimetype = loadEntry("mimetype");
            auto it = MIMETYPES.find(*mimetype);
            if (it == MIMETYPES.end()) {
                return false;
            }
            meta.type = it->second;
        }

        meta.encrypted = false;

        if (isFile("META-INF/manifest.xml")) {
            auto manifest = loadXML("META-INF/manifest.xml");

            tinyxml2::XMLElement *e = manifest
                    ->FirstChildElement("manifest:manifest")
                    ->FirstChildElement("manifest:file-entry");
            for (; e != nullptr; e = e->NextSiblingElement("manifest:file-entry")) {
                std::string fullPath = e->FindAttribute("manifest:full-path")->Value();

                if (fullPath == "/") {
                    //meta.mediaType = e->FindAttribute("manifest:media-type")->Value();
                    //meta.version = e->FindAttribute("manifest:version")->Value();
                    continue;
                }

                auto entryIt = entries.find(fullPath);
                if (entryIt == entries.end()) {
                    continue;
                }

                OpenDocumentEntry &entry = entryIt->second;
                entry.path = fullPath;
                entry.mediaType = e->FindAttribute("manifest:media-type")->Value();

                tinyxml2::XMLElement *crypto = e->FirstChildElement("manifest:encryption-data");
                if (crypto == nullptr) {
                    continue;
                }

                meta.encrypted = true;
                entry.encrypted = true;
                entry.size_real = e->FindAttribute("manifest:size")->UnsignedValue();

#ifdef ODR_CRYPTO
                tinyxml2::XMLElement *algo = crypto->FirstChildElement("manifest:algorithm");
                tinyxml2::XMLElement *key = crypto->FirstChildElement("manifest:key-derivation");
                tinyxml2::XMLElement *start = crypto->FirstChildElement("manifest:start-key-generation");

                std::string checksumType = crypto->FindAttribute("manifest:checksum-type")->Value();
                entry.checksum = crypto->FindAttribute("manifest:checksum")->Value();
                std::string algorithmName = algo->FindAttribute("manifest:algorithm-name")->Value();
                entry.initialisationVector = algo->FindAttribute("manifest:initialisation-vector")->Value();
                std::string keyDerivationName = key->FindAttribute("manifest:key-derivation-name")->Value();
                entry.keySize = key->FindAttribute("manifest:key-size")->UnsignedValue();
                entry.keyIterationCount = key->FindAttribute("manifest:iteration-count")->UnsignedValue();
                entry.keySalt = key->FindAttribute("manifest:salt")->Value();
                std::string startKeyGenerationName = start->FindAttribute("manifest:start-key-generation-name")->Value();
                entry.startKeySize = start->FindAttribute("manifest:key-size")->UnsignedValue();

                auto checksum_it = CHECKSUM_TYPES.find(checksumType);
                auto algorithm_it = ALGORITHM_TYPES.find(algorithmName);
                auto keyDerivation_it = KEY_DERIVATION_TYPES.find(keyDerivationName);
                auto startKey_it = STARTKEY_TYPES.find(startKeyGenerationName);

                if (checksum_it != CHECKSUM_TYPES.end()) {
                    entry.checksumType = checksum_it->second;
                } else {
                    entry.checksumType = ChecksumType::UNKNOWN;
                    LOG(ERROR) << "unknown checksum " << checksumType;
                }
                if (algorithm_it != ALGORITHM_TYPES.end()) {
                    entry.algorithm = algorithm_it->second;
                } else {
                    entry.algorithm = AlgorithmType::UNKNOWN;
                    LOG(ERROR) << "unknown algorithm " << algorithmName;
                }
                if (keyDerivation_it != KEY_DERIVATION_TYPES.end()) {
                    entry.keyDerivation = keyDerivation_it->second;
                } else {
                    entry.keyDerivation = KeyDerivationType::UNKNOWN;
                    LOG(ERROR) << "unknown key derivation " << keyDerivationName;
                }
                if (startKey_it != STARTKEY_TYPES.end()) {
                    entry.startKeyGeneration = startKey_it->second;
                } else {
                    entry.startKeyGeneration = ChecksumType::UNKNOWN;
                    LOG(ERROR) << "unknown start key generation " << startKeyGenerationName;
                }

                entry.checksum = CryptoUtil::base64Decode(entry.checksum);
                entry.initialisationVector = CryptoUtil::base64Decode(entry.initialisationVector);
                entry.keySalt = CryptoUtil::base64Decode(entry.keySalt);
#endif

                if ((smallestEncryptedEntry == nullptr) || (entry.size_uncompressed < smallestEncryptedEntry->size_uncompressed)) {
                    smallestEncryptedEntry = &entry;
                }
            }
        }

        return meta.type != FileType::UNKNOWN;
    }

    void createMeta2() {
        if (isFile("meta.xml")) {
            auto metaXml = loadXML("meta.xml");
            tinyxml2::XMLHandle metaHandle(metaXml.get());

            tinyxml2::XMLElement *metaElement = metaHandle
                    .FirstChildElement("office:document-meta")
                    .ToElement();

            tinyxml2::XMLElement *statisticsElement = metaHandle
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

        if (isFile("content.xml")) {
            // TODO: dont load content twice (happens in case of translation)
            auto contentXml = loadXML("content.xml");
            tinyxml2::XMLHandle contentHandle(contentXml.get());
            tinyxml2::XMLHandle bodyHandle = contentHandle
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
    }

    bool open(const std::string &path) {
        memset(&zip, 0, sizeof(zip));
        mz_bool status = mz_zip_reader_init_file(&zip, path.data(), MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
        if (!status) {
            LOG(ERROR) << "miniz error! not a file or not a zip file?";
            return false;
        }

        if (!createEntries()){
            LOG(ERROR) << "could not create entry table!";
            return false;
        }
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
        switch (smallestEncryptedEntry->startKeyGeneration) {
            case ChecksumType::SHA1:
                startKey = CryptoUtil::sha1(password);
                break;
            case ChecksumType::SHA256:
                startKey = CryptoUtil::sha256(password);
                break;
            default:
                throw;
        }
        if (startKey.size() < smallestEncryptedEntry->startKeySize) {
            LOG(ERROR) << "start key too short";
            return false;
        }
        startKey = startKey.substr(0, smallestEncryptedEntry->startKeySize);
        decrypted = validatePassword(*smallestEncryptedEntry);
        if (decrypted) {
            createMeta2();
        }
        return decrypted;
#else
        return false;
#endif
    }

#ifdef ODR_CRYPTO
    bool canDecrypt(const OpenDocumentEntry &entry) {
        return (entry.checksumType != ChecksumType::UNKNOWN) &&
                (entry.algorithm != AlgorithmType::UNKNOWN) &&
                (entry.keyDerivation != KeyDerivationType::UNKNOWN) &&
                (entry.startKeyGeneration != ChecksumType::UNKNOWN);
    }

    std::string deriveKeyAndDecrypt(const OpenDocumentEntry &entry, const std::string &input) {
        std::string derivedKey = CryptoUtil::pbkdf2(entry.keySize, startKey, entry.keySalt, entry.keyIterationCount);
        switch (entry.algorithm) {
            case AlgorithmType::AES256_CBC:
                return CryptoUtil::decryptAES(derivedKey, entry.initialisationVector, input);
            case AlgorithmType::TRIPLE_DES_CBC:
                return CryptoUtil::decryptTripleDES(derivedKey, entry.initialisationVector, input);
            case AlgorithmType::BLOWFISH_CFB:
                return CryptoUtil::decryptBlowfish(derivedKey, entry.initialisationVector, input);
            default:
                throw;
        }
    }

    bool validatePassword(const OpenDocumentEntry &entry) {
        std::string result = deriveKeyAndDecrypt(entry, loadPlain(entry));
        std::size_t padding = CryptoUtil::padding(result);
        result = result.substr(0, result.size() - padding);
        std::string checksum;
        switch (entry.checksumType) {
            case ChecksumType::SHA1:
                checksum = CryptoUtil::sha1(result);
                break;
            case ChecksumType::SHA1_1K:
                checksum = CryptoUtil::sha1(result.substr(0, 1024));
                break;
            case ChecksumType::SHA256_1K:
                checksum = CryptoUtil::sha256(result.substr(0, 1024));
                break;
            default:
                throw;
        }
        return checksum == entry.checksum;
    }
#endif

    void close() {
        if (!opened) {
            return;
        }
        mz_zip_reader_end(&zip);
        entries.clear();
        opened = false;
        decrypted = false;
        smallestEncryptedEntry = nullptr;
    }

    std::string normalizePath(const std::string &path) const {
        std::string result;
        if (path.rfind("./", 0) == 0) {
            result = path.substr(2);
        } else {
            result = path;
        }
        return result;
    }

    bool isFile(const std::string &path) const {
        std::string npath = normalizePath(path);
        return entries.find(npath) != entries.end();
    }

    std::string loadPlain(const OpenDocumentEntry &entry) {
        auto reader = mz_zip_reader_extract_iter_new(&zip, entry.index, 0);
        std::string result(entry.size_uncompressed, '\0');
        auto read = mz_zip_reader_extract_iter_read(reader, &result[0], entry.size_uncompressed);
        mz_zip_reader_extract_iter_free(reader);
        return result;
    }

    std::unique_ptr<std::string> loadEntry(const std::string &path) {
        std::string npath = normalizePath(path);
        auto it = entries.find(npath);
        if (it == entries.end()) {
            LOG(ERROR) << "zip entry size not found " << path;
            return nullptr;
        }
        std::string result = loadPlain(it->second);
        if (result.size() != it->second.size_uncompressed) {
            LOG(ERROR) << "zip entry size doesn't match " << path;
            return nullptr;
        }
#ifdef ODR_CRYPTO
        if (it->second.encrypted) {
            if (!decrypted) {
                LOG(ERROR) << "not decrypted";
                return nullptr;
            }
            if (!canDecrypt(it->second)) {
                LOG(ERROR) << "cannot decrypt " << path;
                return nullptr;
            }
            result = CryptoUtil::inflate(deriveKeyAndDecrypt(it->second, result));
            if (result.size() != it->second.size_real) {
                LOG(ERROR) << "inflated size doesn't match " << path;
                return nullptr;
            }
        }
#endif
        return std::make_unique<std::string>(result);
    }

    std::unique_ptr<tinyxml2::XMLDocument> loadXML(const std::string &path) {
        auto xml = loadEntry(path);
        if (!xml) {
            return nullptr;
        }
        auto result = std::make_unique<tinyxml2::XMLDocument>();
        result->Parse(xml->data(), xml->size());
        return result;
    }
};

OpenDocumentFile::OpenDocumentFile() :
        impl_(new OpenDocumentFileImpl()) {
}

OpenDocumentFile::~OpenDocumentFile() {
    delete impl_;
}

bool OpenDocumentFile::open(const std::string &path) {
    return impl_->open(path);
}

bool OpenDocumentFile::decrypt(const std::string &password) {
    return impl_->decrypt(password);
}

void OpenDocumentFile::close() {
    return impl_->close();
}

bool OpenDocumentFile::isOpen() const {
    return impl_->opened;
}

bool OpenDocumentFile::isDecrypted() const {
    return impl_->decrypted;
}

const Entries OpenDocumentFile::getEntries() const {
    return impl_->entries;
}

const FileMeta &OpenDocumentFile::getMeta() const {
    return impl_->meta;
}

bool OpenDocumentFile::isFile(const std::string &path) const {
    return impl_->isFile(path);
}

std::unique_ptr<std::string> OpenDocumentFile::loadEntry(const std::string &path) {
    return impl_->loadEntry(path);
}

std::unique_ptr<tinyxml2::XMLDocument> OpenDocumentFile::loadXML(const std::string &path) {
    return impl_->loadXML(path);
}

}
