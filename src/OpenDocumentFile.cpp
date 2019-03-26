#include "OpenDocumentFile.h"
#include <fstream>
#include "miniz_zip.h"
#include "tinyxml2.h"
#ifdef ODR_CRYPTO
#include "openssl/evp.h"
#endif

namespace odr {

static const std::map<std::string, OpenDocumentFile::Meta::Type> MIMETYPES = {
        {"application/vnd.oasis.opendocument.text", OpenDocumentFile::Meta::Type::TEXT},
        {"application/vnd.oasis.opendocument.spreadsheet", OpenDocumentFile::Meta::Type::SPREADSHEET},
        {"application/vnd.oasis.opendocument.presentation", OpenDocumentFile::Meta::Type::PRESENTATION},
};

class OpenDocumentFileImpl : public OpenDocumentFile {
public:
    mz_zip_archive zip;
    Meta meta;
    Entries entries;

    explicit OpenDocumentFileImpl(const std::string &path) {
        memset(&zip, 0, sizeof(zip));
        mz_bool status = mz_zip_reader_init_file(&zip, path.c_str(), MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
        if (!status) {
            throw "miniz error! not a file or not a zip file?";
        }

        if (!createEntries()){
            throw "could not create entry table!";
        }
        if (!createMeta()) {
            throw "could not get meta!";
        }
    }

    ~OpenDocumentFileImpl() override {
        destroyMeta();
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
            entry.size = entry_stat.m_uncomp_size;
            entry.size_compressed = entry_stat.m_comp_size;
            entry.index = i;
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
            auto it = MIMETYPES.find(mimetype);
            if (it == MIMETYPES.end()) {
                return false;
            }
            meta.type = it->second;
        }

        if (isFile("META-INF/manifest.xml")) {
            auto manifest = loadXML("META-INF/manifest.xml");
            // TODO: parse version
            // TODO: parse media type
            // TODO: parse entity media types
            // TODO: parse entity crypto info
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
                    case Meta::Type::TEXT: {
                        const tinyxml2::XMLAttribute *pageCount = statisticsElement->FindAttribute("meta:page-count");
                        if (pageCount == nullptr) {
                            break;
                        }
                        meta.text.pageCount = pageCount->UnsignedValue();
                    } break;
                    case Meta::Type::SPREADSHEET: {
                        const tinyxml2::XMLAttribute *tableCount = statisticsElement->FindAttribute("meta:table-count");
                        if (tableCount == nullptr) {
                            break;
                        }
                        meta.spreadsheet.tableCount = tableCount->UnsignedValue();
                        meta.spreadsheet.tables = new Meta::Spreadsheet::Table[meta.spreadsheet.tableCount];
                    } break;
                    case Meta::Type::PRESENTATION: {
                        meta.presentation.pageCount = 0;
                    } break;
                    default:
                        break;
                }
            }
        }

        return meta.type != Meta::Type::UNKNOWN;
    }

    void destroyMeta() {
        if (meta.type == Meta::Type::UNKNOWN) {
            delete[] meta.spreadsheet.tables;
        }
    }

    const Entries getEntries() const override {
        return entries;
    }

    const Meta &getMeta() const override {
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
        std::string result(it->second.size, '\0');
        auto read = mz_zip_reader_extract_iter_read(reader, &result[0], it->second.size);
        if (read != it->second.size) {
            return nullptr;
        }
        mz_zip_reader_extract_iter_free(reader);
        return result;
    }

    std::unique_ptr<tinyxml2::XMLDocument> loadXML(const std::string &path) {
        if (!isFile(path)) {
            return nullptr;
        }
        auto result = std::make_unique<tinyxml2::XMLDocument>();
        auto xml = loadText(path);
        result->Parse(xml.c_str(), xml.size());
        return result;
    }

    void close() override {
        mz_zip_reader_end(&zip);
    }
};

std::unique_ptr<OpenDocumentFile> OpenDocumentFile::open(const std::string &path) {
    return std::make_unique<OpenDocumentFileImpl>(path);
}

}
