#include "MicrosoftOpenXmlFile.h"
#include <fstream>
#include <algorithm>
#include "miniz_zip.h"
#include "tinyxml2.h"
#include "glog/logging.h"

namespace odr {

typedef MicrosoftOpenXmlFile::Entries Entries;

static const std::map<std::string, FileType> TYPES = {
        {"word/document.xml", FileType::OFFICE_OPEN_XML_DOCUMENT},
        {"ppt/presentation.xml", FileType::OFFICE_OPEN_XML_PRESENTATION},
        {"xl/workbook.xml", FileType::OFFICE_OPEN_XML_WORKBOOK},
};

class MicrosoftOpenXmlFileImpl {
public:
    mz_zip_archive zip;
    FileMeta meta;
    Entries entries;
    std::string startKey;

    bool opened = false;
    bool decrypted = false;

    ~MicrosoftOpenXmlFileImpl() {
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

            MicrosoftOpenXmlEntry entry;
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
        for (auto &&t : TYPES) {
            if (isFile(t.first)) {
                meta.type = t.second;
                break;
            }
        }

        decrypted = true;

        return meta.type != FileType::UNKNOWN;
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

        opened = true;
        return true;
    }

    bool decrypt(const std::string &password) {
        return false;
    }

    void close() {
        if (!opened) {
            return;
        }
        mz_zip_reader_end(&zip);
        entries.clear();
        opened = false;
        decrypted = false;
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

    std::string loadPlain(const MicrosoftOpenXmlEntry &entry) {
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

MicrosoftOpenXmlFile::MicrosoftOpenXmlFile() :
        impl_(new MicrosoftOpenXmlFileImpl()) {
}

MicrosoftOpenXmlFile::~MicrosoftOpenXmlFile() {
    delete impl_;
}

bool MicrosoftOpenXmlFile::open(const std::string &path) {
    return impl_->open(path);
}

bool MicrosoftOpenXmlFile::decrypt(const std::string &password) {
    return impl_->decrypt(password);
}

void MicrosoftOpenXmlFile::close() {
    impl_->close();
}

bool MicrosoftOpenXmlFile::isOpen() const {
    return impl_->opened;
}

bool MicrosoftOpenXmlFile::isDecrypted() const {
    return impl_->decrypted;
}

const Entries MicrosoftOpenXmlFile::getEntries() const {
    return impl_->entries;
}

const FileMeta& MicrosoftOpenXmlFile::getMeta() const {
    return impl_->meta;
}

bool MicrosoftOpenXmlFile::isFile(const std::string &path) const {
    return impl_->isFile(path);
}

std::unique_ptr<std::string> MicrosoftOpenXmlFile::loadEntry(const std::string &path) {
    return impl_->loadEntry(path);
}

std::unique_ptr<tinyxml2::XMLDocument> MicrosoftOpenXmlFile::loadXML(const std::string &path) {
    return impl_->loadXML(path);
}

}
