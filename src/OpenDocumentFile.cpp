#include "OpenDocumentFile.h"
#include <fstream>

namespace odr {

const std::map<std::string, OpenDocumentFile::Meta::Type> OpenDocumentFile::MIMETYPES = {
        {"application/vnd.oasis.opendocument.text", OpenDocumentFile::Meta::Type::TEXT},
        {"application/vnd.oasis.opendocument.spreadsheet", OpenDocumentFile::Meta::Type::SPREADSHEET},
        {"application/vnd.oasis.opendocument.presentation", OpenDocumentFile::Meta::Type::PRESENTATION},
};

OpenDocumentFile::OpenDocumentFile(const std::string &path) {
    memset(&_zip, 0, sizeof(_zip));
    mz_bool status = mz_zip_reader_init_file(&_zip, path.c_str(), MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
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

OpenDocumentFile::~OpenDocumentFile() {
    destroyMeta();
    close();
}

bool OpenDocumentFile::createEntries() {
    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&_zip); ++i) {
        mz_zip_archive_file_stat entry_stat;
        if (!mz_zip_reader_file_stat(&_zip, i, &entry_stat)) {
            mz_zip_reader_end(&_zip);
            return false;
        }

        if (mz_zip_reader_is_file_a_directory(&_zip, i)) {
            continue;
        }

        Entry entry;
        entry.size = entry_stat.m_uncomp_size;
        entry.size_compressed = entry_stat.m_comp_size;
        entry.index = i;
        _entries[entry_stat.m_filename] = entry;
    }

    return true;
}

bool OpenDocumentFile::createMeta() {
    if (!isFile("content.xml") | !isFile("styles.xml")) {
        return false;
    }

    if (isFile("mimetype")) {
        auto mimetype = loadText("mimetype");
        auto it = MIMETYPES.find(*mimetype);
        if (it == MIMETYPES.end()) {
            return false;
        }
        _meta.type = it->second;
    }

    if (isFile("META-INF/manifest.xml")) {
        auto manifest = loadXML("META-INF/manifest.xml");
        // TODO: parse version
        // TODO: parse media type
        // TODO: parse entity media types
        // TODO: parse entity crypto info
    }

    if (isFile("meta.xml")) {
        auto meta = loadXML("meta.xml");
        tinyxml2::XMLHandle metaHandle(meta.get());
        tinyxml2::XMLElement *statisticsElement = metaHandle
                .FirstChildElement("office:document-meta")
                .FirstChildElement("office:meta")
                .FirstChildElement("meta:document-statistic")
                .ToElement();
        if (statisticsElement != nullptr) {
            switch (_meta.type) {
                case Meta::Type::TEXT: {
                    const tinyxml2::XMLAttribute *pageCount = statisticsElement->FindAttribute("meta:page-count");
                    if (pageCount == nullptr) {
                        break;
                    }
                    _meta.text.pageCount = pageCount->UnsignedValue();
                } break;
                case Meta::Type::SPREADSHEET: {
                    const tinyxml2::XMLAttribute *tableCount = statisticsElement->FindAttribute("meta:table-count");
                    if (tableCount == nullptr) {
                        break;
                    }
                    _meta.spreadsheet.tableCount = tableCount->UnsignedValue();
                    _meta.spreadsheet.tables = new Meta::Spreadsheet::Table[_meta.spreadsheet.tableCount];
                } break;
                case Meta::Type::PRESENTATION: {
                    _meta.presentation.pageCount = 0;
                } break;
                default:
                    break;
            }
        }
    }

    return _meta.type != Meta::Type::UNKNOWN;
}

void OpenDocumentFile::destroyMeta() {
    if (_meta.type == Meta::Type::UNKNOWN) {
        delete[] _meta.spreadsheet.tables;
    }
}

const OpenDocumentFile::Entries OpenDocumentFile::getEntries() const {
    return _entries;
}

const OpenDocumentFile::Meta &OpenDocumentFile::getMeta() const {
    return _meta;
}

bool OpenDocumentFile::isFile(const std::string &path) const {
    return _entries.find(path) != _entries.end();
}

std::unique_ptr<std::string> OpenDocumentFile::loadText(const std::string &path) {
    auto it = _entries.find(path);
    if (it == _entries.end()) {
        return nullptr;
    }
    auto reader = mz_zip_reader_extract_iter_new(&_zip, it->second.index, 0);
    std::string result(it->second.size, '\0');
    auto read = mz_zip_reader_extract_iter_read(reader, &result[0], it->second.size);
    if (read != it->second.size) {
        return nullptr;
    }
    mz_zip_reader_extract_iter_free(reader);
    return std::make_unique<std::string>(std::move(result));
}

std::unique_ptr<tinyxml2::XMLDocument> OpenDocumentFile::loadXML(const std::string &path) {
    if (!isFile(path)) {
        return nullptr;
    }
    auto result = std::make_unique<tinyxml2::XMLDocument>();
    auto xml = loadText(path);
    result->Parse(xml->c_str(), xml->size());
    return result;
}

void OpenDocumentFile::close() {
    mz_zip_reader_end(&_zip);
}

}
