#include "OpenDocumentFile.h"
#include <fstream>
#include "miniz_zip.h"
#include "tinyxml2.h"
#include "glog/logging.h"

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
                        // TODO: use content.xml?
                        meta.entryCount = tableCount->UnsignedValue();
                        // TODO: get table names
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
};

std::unique_ptr<OpenDocumentFile> OpenDocumentFile::create() {
    return std::make_unique<OpenDocumentFileImpl>();
}

}
