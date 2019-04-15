#include "OpenDocumentFile.h"
#include <fstream>
#include "miniz_zip.h"
#include "cryptopp/hex.h"
#include "cryptopp/filters.h"
#include "cryptopp/base64.h"
#include "cryptopp/pwdbased.h"
#include "cryptopp/sha.h"
#include "cryptopp/modes.h"
#include "cryptopp/aes.h"
#include "cryptopp/zinflate.h"
#include "tinyxml2.h"
#include "glog/logging.h"

typedef unsigned char byte;

static std::string base64decode(const std::string &in) {
    std::string out;
    CryptoPP::Base64Decoder b(new CryptoPP::StringSink(out));
    b.Put((const byte *) in.data(), in.size());
    b.MessageEnd();
    return out;
}

static std::string sha256(const std::string &in) {
    byte out[CryptoPP::SHA256::DIGESTSIZE];
    CryptoPP::SHA256().CalculateDigest(out, (byte *) in.data(), in.size());
    return std::string((char *) out, CryptoPP::SHA256::DIGESTSIZE);
}

namespace odr {

static const std::map<std::string, DocumentType> MIMETYPES = {
        {"application/vnd.oasis.opendocument.text", DocumentType::TEXT},
        {"application/vnd.oasis.opendocument.spreadsheet", DocumentType::SPREADSHEET},
        {"application/vnd.oasis.opendocument.presentation", DocumentType::PRESENTATION},
};

class OpenDocumentFileImpl : public OpenDocumentFile {
public:
    mz_zip_archive zip;
    DocumentMeta meta;
    Entries entries;

    ~OpenDocumentFileImpl() override {
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

            Entry entry;
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
            auto mimetype = loadText("mimetype");
            meta.mediaType = mimetype;
            auto it = MIMETYPES.find(mimetype);
            if (it == MIMETYPES.end()) {
                return false;
            }
            meta.type = it->second;
        }

        meta.encrypted = false;

        if (isFile("META-INF/manifest.xml")) {
            auto manifest = loadXML("META-INF/manifest.xml");

            // TODO: pointer safe
            tinyxml2::XMLElement *e = manifest
                    ->FirstChildElement("manifest:manifest")
                    ->FirstChildElement("manifest:file-entry");
            for (; e != nullptr; e = e->NextSiblingElement("manifest:file-entry")) {
                std::string fullPath = e->FindAttribute("manifest:full-path")->Value();

                if (fullPath == "/") {
                    meta.mediaType = e->FindAttribute("manifest:media-type")->Value();
                    meta.version = e->FindAttribute("manifest:version")->Value();
                    continue;
                }

                auto entryIt = entries.find(fullPath);
                if (entryIt == entries.end()) {
                    continue;
                }

                meta.encrypted = true;
                Entry &entry = entryIt->second;
                entry.encrypted = true;

                entry.size_real = e->FindAttribute("manifest:size")->UnsignedValue();
                entry.mediaType = e->FindAttribute("manifest:media-type")->Value();

                tinyxml2::XMLElement *crypto = e->FirstChildElement("manifest:encryption-data");
                if (crypto == nullptr) {
                    continue;
                }

                tinyxml2::XMLElement *algo = crypto->FirstChildElement("manifest:algorithm");
                tinyxml2::XMLElement *key = crypto->FirstChildElement("manifest:key-derivation");
                tinyxml2::XMLElement *start = crypto->FirstChildElement("manifest:start-key-generation");

                entry.checksumType = crypto->FindAttribute("manifest:checksum-type")->Value();
                entry.checksum = crypto->FindAttribute("manifest:checksum")->Value();
                entry.algorithmName = algo->FindAttribute("manifest:algorithm-name")->Value();
                entry.initialisationVector = algo->FindAttribute("manifest:initialisation-vector")->Value();
                entry.keyDerivationName = key->FindAttribute("manifest:key-derivation-name")->Value();
                entry.keySize = key->FindAttribute("manifest:key-size")->UnsignedValue();
                entry.keyIterationCount = key->FindAttribute("manifest:iteration-count")->UnsignedValue();
                entry.keySalt = key->FindAttribute("manifest:salt")->Value();
                entry.startKeyGenerationName = start->FindAttribute("manifest:start-key-generation-name")->Value();
                entry.startKeySize = start->FindAttribute("manifest:key-size")->Value();

                entry.checksum = base64decode(entry.checksum);
                entry.initialisationVector = base64decode(entry.initialisationVector);
                entry.keySalt = base64decode(entry.keySalt);
            }
        }

        if (isFile("meta.xml")) {
            auto metaXml = loadXML("meta.xml");
            tinyxml2::XMLHandle metaHandle(metaXml.get());
            tinyxml2::XMLElement *statisticsElement = metaHandle
                    .FirstChildElement("office:document-meta")
                    .FirstChildElement("office:meta")
                    .FirstChildElement("meta:document-statistic")
                    .ToElement();
            if (statisticsElement != nullptr) {
                switch (meta.type) {
                    case DocumentType::TEXT: {
                        const tinyxml2::XMLAttribute *pageCount = statisticsElement->FindAttribute("meta:page-count");
                        if (pageCount == nullptr) {
                            break;
                        }
                        meta.entryCount = pageCount->UnsignedValue();
                    } break;
                    case DocumentType::SPREADSHEET: {
                        const tinyxml2::XMLAttribute *tableCount = statisticsElement->FindAttribute("meta:table-count");
                        if (tableCount == nullptr) {
                            break;
                        }
                        meta.entryCount = tableCount->UnsignedValue();
                    } break;
                    case DocumentType::PRESENTATION: {
                        meta.entryCount = 0;
                    } break;
                    default:
                        break;
                }
            }
        }

        if (!isFile("content.xml")) {
            return false;
        } else {
            // TODO: dont load content twice (happens in case of translation)
            auto contentXml = loadXML("content.xml");
            tinyxml2::XMLHandle contentHandle(contentXml.get());
            tinyxml2::XMLHandle bodyHandle = contentHandle
                    .FirstChildElement("office:document-content")
                    .FirstChildElement("office:body");
            if (bodyHandle.ToElement() != nullptr) {
                switch (meta.type) {
                    case DocumentType::SPREADSHEET: {
                        meta.entryCount = 0;
                        tinyxml2::XMLHandle tableHandle = bodyHandle
                                .FirstChildElement("office:spreadsheet")
                                .FirstChildElement("table:table");
                        while (tableHandle.ToElement() != nullptr) {
                            ++meta.entryCount;
                            DocumentMeta::Entry entry;
                            entry.name = tableHandle.ToElement()->FindAttribute("table:name")->Value();
                            // TODO: table dimension
                            meta.entries.emplace_back(entry);
                            tableHandle = tableHandle.NextSiblingElement("table:table");
                        }
                    } break;
                    case DocumentType::PRESENTATION: {
                        meta.entryCount = 0;
                        tinyxml2::XMLHandle pageHandle = bodyHandle
                                .FirstChildElement("office:presentation")
                                .FirstChildElement("draw:page");
                        while (pageHandle.ToElement() != nullptr) {
                            ++meta.entryCount;
                            DocumentMeta::Entry entry;
                            entry.name = pageHandle.ToElement()->FindAttribute("draw:name")->Value();
                            meta.entries.emplace_back(entry);
                            pageHandle = pageHandle.NextSiblingElement("draw:page");
                        }
                    } break;
                    default:
                        break;
                }
            }
        }

        return meta.type != DocumentType::UNKNOWN;
    }

    bool open(const std::string &path) override {
        memset(&zip, 0, sizeof(zip));
        mz_bool status = mz_zip_reader_init_file(&zip, path.c_str(), MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
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

        return true;
    }

    bool decrypt(const std::string &password) override {
        // TODO
        return true;
    }

    void close() override {
        mz_zip_reader_end(&zip);
    }

    const Entries getEntries() const override {
        return entries;
    }

    const DocumentMeta &getMeta() const override {
        return meta;
    }

    bool isFile(const std::string &path) const override {
        return entries.find(path) != entries.end();
    }

    std::string loadText(const std::string &path) override {
        auto it = entries.find(path);
        if (it == entries.end()) {
            return nullptr;
        }
        auto reader = mz_zip_reader_extract_iter_new(&zip, it->second.index, 0);
        std::string result(it->second.size_uncompressed, '\0');
        auto read = mz_zip_reader_extract_iter_read(reader, &result[0], it->second.size_uncompressed);
        if (read != it->second.size_uncompressed) {
            return nullptr;
        }
        mz_zip_reader_extract_iter_free(reader);
        return result;
    }

    std::unique_ptr<tinyxml2::XMLDocument> loadXML(const std::string &path) override {
        if (!isFile(path)) {
            return nullptr;
        }
        if (entries[path].encrypted) {
            // TODO
            return nullptr;
        }
        auto result = std::make_unique<tinyxml2::XMLDocument>();
        auto xml = loadText(path);
        result->Parse(xml.c_str(), xml.size());
        return result;
    }
};

std::unique_ptr<OpenDocumentFile> OpenDocumentFile::create() {
    return std::make_unique<OpenDocumentFileImpl>();
}

}
