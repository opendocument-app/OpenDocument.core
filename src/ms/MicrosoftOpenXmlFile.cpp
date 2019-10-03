#include "MicrosoftOpenXmlFile.h"
#include <cstdint>
#include <unordered_map>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/FileMeta.h"
#include "../io/Path.h"
#include "../io/ZipStorage.h"

namespace odr {

namespace {
struct Entry {
    Path path;
    std::size_t sizeReal;
    std::size_t sizeUncompressed;
    std::size_t sizeCompressed;
    uint32_t index;
    std::string mediaType;
    bool encrypted;
};
}

class MicrosoftOpenXmlFile::Impl final {
public:
    static const std::unordered_map<Path, FileType> &getTypes() {
        static const std::unordered_map<Path, FileType> TYPES = {
                {"word/document.xml", FileType::OFFICE_OPEN_XML_DOCUMENT},
                {"ppt/presentation.xml", FileType::OFFICE_OPEN_XML_PRESENTATION},
                {"xl/workbook.xml", FileType::OFFICE_OPEN_XML_WORKBOOK},
        };
        return TYPES;
    }

    std::unique_ptr<ZipReader> zip;
    FileMeta meta;
    std::unordered_map<Path, Entry> entries;
    Path contentPath;

    bool opened = false;
    bool decrypted = false;

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
        for (auto &&t : getTypes()) {
            if (isFile(t.first)) {
                meta.type = t.second;
                contentPath = t.first;
                break;
            }
        }

        decrypted = true;

        return meta.type != FileType::UNKNOWN;
    }

    bool open(const Path &path) {
        zip = std::make_unique<ZipReader>(path);

        createEntries();
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

        zip = nullptr;
        entries.clear();
        opened = false;
        decrypted = false;
    }

    bool isFile(const Path &path) const {
        return zip->isFile(path);
    }

    const Path &getContentPath() const {
        return contentPath;
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
            LOG(ERROR) << "zip entry size not found " << path;
            return ""; // TODO throw
        }
        const std::string result = loadPlain(it->second);
        if (result.size() != it->second.sizeUncompressed) {
            LOG(ERROR) << "zip entry size doesn't match " << path;
            return ""; // TODO throw
        }
        return result;
    }

    std::unique_ptr<tinyxml2::XMLDocument> loadXml(const Path &path) {
        if (!zip->isFile(path)) return nullptr;
        const auto xml = loadEntry(path);
        auto result = std::make_unique<tinyxml2::XMLDocument>();
        result->Parse(xml.data(), xml.size());
        return result;
    }

    std::unique_ptr<tinyxml2::XMLDocument> loadRelationships(const Path &path) {
        if (!zip->isFile(path)) return nullptr;
        return loadXml(path.parent().join("_rels").join(path.basename() + ".rels"));
    }

    std::unique_ptr<tinyxml2::XMLDocument> loadContent() {
        return loadXml(getContentPath());
    }
};

MicrosoftOpenXmlFile::MicrosoftOpenXmlFile() :
        impl(new Impl()) {
}

MicrosoftOpenXmlFile::~MicrosoftOpenXmlFile() = default;

bool MicrosoftOpenXmlFile::open(const Path &path) {
    return impl->open(path);
}

bool MicrosoftOpenXmlFile::decrypt(const std::string &password) {
    return impl->decrypt(password);
}

void MicrosoftOpenXmlFile::close() {
    impl->close();
}

bool MicrosoftOpenXmlFile::isOpen() const {
    return impl->opened;
}

bool MicrosoftOpenXmlFile::isDecrypted() const {
    return impl->decrypted;
}

const FileMeta &MicrosoftOpenXmlFile::getMeta() const {
    return impl->meta;
}

bool MicrosoftOpenXmlFile::isFile(const Path &path) const {
    return impl->isFile(path);
}

const Path &MicrosoftOpenXmlFile::getContentPath() const {
    return impl->getContentPath();
}

std::string MicrosoftOpenXmlFile::loadEntry(const Path &path) {
    return impl->loadEntry(path);
}

std::unique_ptr<tinyxml2::XMLDocument> MicrosoftOpenXmlFile::loadXml(const Path &path) {
    return impl->loadXml(path);
}

std::unique_ptr<tinyxml2::XMLDocument> MicrosoftOpenXmlFile::loadRelationships(const Path &path) {
    return impl->loadRelationships(path);
}

std::unique_ptr<tinyxml2::XMLDocument> MicrosoftOpenXmlFile::loadContent() {
    return impl->loadContent();
}

const ZipReader &MicrosoftOpenXmlFile::getZipReader() const {
    return *impl->zip;
}

}
