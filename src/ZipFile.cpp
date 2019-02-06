#include "ZipFile.h"

#ifdef USE_MINIZ

#include <list>
#include <set>
#include "miniz_zip.h"

namespace opendocument {

class ZipFileMinizImpl final : public ZipFile {
public:
    struct Entry : ZipFile::Entry {
        mz_uint file_index;

        bool operator < (const Entry &other) const { return name < other.name; }
    };
    typedef std::set<Entry> Entries;

    explicit ZipFileMinizImpl(const std::string &);
    ~ZipFileMinizImpl() override = default;

    bool exists(const Path &) override;
    bool isFile(const Path &) override;
    bool isDirectory(const Path &) override;
    Size getSize(const Path &) override;
    std::unique_ptr<Source> read(const Path &) override;
    void close() override;
private:
    void createEntries();
    const Entry *findEntry(const Path &) const;

    mz_zip_archive _zip;
    Entries _entries;
    std::list<Source *> _sources;
};

class SourceMinizImpl : public Source {
public:
    SourceMinizImpl(mz_zip_archive *zip, mz_uint file_index);
    ~SourceMinizImpl() override = default;

    Size read(Byte *buffer, Size size) override;
    void close() override;

private:
    mz_zip_reader_extract_iter_state *_reader;
};

ZipFileMinizImpl::ZipFileMinizImpl(const std::string &path) {
    memset(&_zip, 0, sizeof(_zip));
    mz_bool status = mz_zip_reader_init_file(&_zip, path.c_str(), MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
    if (!status) {
        // TODO: throw
        throw;
    }
    createEntries();
}

void ZipFileMinizImpl::createEntries() {
    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&_zip); ++i) {
        mz_zip_archive_file_stat entry_stat;
        if (!mz_zip_reader_file_stat(&_zip, i, &entry_stat)) {
            mz_zip_reader_end(&_zip);
            // TODO: throw
        }
        Entry entry;
        entry.directory = (bool) mz_zip_reader_is_file_a_directory(&_zip, i);
        entry.name = entry_stat.m_filename;
        entry.comment = entry_stat.m_comment;
        entry.size_compressed = entry_stat.m_comp_size;
        entry.size_uncompressed = entry_stat.m_uncomp_size;
        entry.file_index = i;
        _entries.insert(entry);
    }
}

const ZipFileMinizImpl::Entry *ZipFileMinizImpl::findEntry(const Path &path) const {
    Entry finder;
    finder.name = path;
    auto it = _entries.find(finder);
    if (it == _entries.end()) {
        return nullptr;
    }
    return &*it;
}

bool ZipFileMinizImpl::exists(const Path &path) {
    auto entry = findEntry(path);
    return entry != nullptr;
}

bool ZipFileMinizImpl::isFile(const Path &path) {
    auto entry = findEntry(path);
    if (entry == nullptr) {
        return false;
    }
    return !entry->directory;
}

bool ZipFileMinizImpl::isDirectory(const Path &path) {
    auto entry = findEntry(path);
    if (entry == nullptr) {
        return false;
    }
    return entry->directory;
}

Size ZipFileMinizImpl::getSize(const Path &path) {
    auto entry = findEntry(path);
    if (entry == nullptr) {
        return -1;
    }
    return entry->size_uncompressed;
}

std::unique_ptr<Source> ZipFileMinizImpl::read(const Path &path) {
    auto entry = findEntry(path);
    return std::make_unique<SourceMinizImpl>(&_zip, entry->file_index);
}

void ZipFileMinizImpl::close() {
    while (!_sources.empty()) {
        auto source = _sources.front();
        source->close();
        delete source;
        _sources.pop_front();
    }

    mz_zip_reader_end(&_zip);
}

SourceMinizImpl::SourceMinizImpl(mz_zip_archive *zip, mz_uint file_index) {
    _reader = mz_zip_reader_extract_iter_new(zip, file_index, 0);
}

Size SourceMinizImpl::read(Byte *buffer, Size size) {
    if (_reader == nullptr) {
        return 0;
    }
    return mz_zip_reader_extract_iter_read(_reader, buffer, (size_t) size);
}

void SourceMinizImpl::close() {
    mz_zip_reader_extract_iter_free(_reader);
    _reader = nullptr;
}

}

#endif

namespace opendocument {

std::unique_ptr<ZipFile> ZipFile::open(const std::string &path) {
    try {
#ifdef USE_MINIZ
        return std::make_unique<ZipFileMinizImpl>(path);
#endif
    } catch (...) {
        return nullptr;
    }
}

}
