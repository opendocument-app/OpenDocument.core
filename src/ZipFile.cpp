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

    explicit ZipFileMinizImpl(const std::string &path);
    ~ZipFileMinizImpl() override;

    size_t size(const Path &path) const override;
    Source &read(const Path &path) override;

    void close() override;
private:
    mz_zip_archive _zip;
    Entries _entries;
    std::list<Source *> _sources;

    void createEntries();

    class ConstIteratorImpl final : public ConstIterator_ {
    public:
        explicit ConstIteratorImpl(Entries::const_iterator it) : _it(it) {}
        ~ConstIteratorImpl() override = default;
        std::unique_ptr<ConstIterator_> copy() const override { return std::make_unique<ConstIteratorImpl>(_it); }
        const ZipFile::Entry &operator*() const override { return *_it; }
        void operator++() override { ++_it; }
        bool operator!=(const ConstIterator_ &it) const override { return _it != ((ConstIteratorImpl &) it)._it; }
    private:
        Entries::const_iterator _it;
    };

    std::unique_ptr<ConstIterator_> begin_() const override { return std::make_unique<ConstIteratorImpl>(_entries.begin()); }
    std::unique_ptr<ConstIterator_> end_() const override { return std::make_unique<ConstIteratorImpl>(_entries.end()); }

    class SourceImpl final : public Source {
    public:
        SourceImpl(mz_zip_archive *zip, mz_uint file_index);
        ~SourceImpl() override { close(); }
        std::size_t read(Byte *buffer, std::size_t size) const override;
        void close() override;
    private:
        mz_zip_reader_extract_iter_state *_reader;
    };
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

ZipFileMinizImpl::~ZipFileMinizImpl() {
    close();
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

size_t ZipFileMinizImpl::size(const Path &path) const {
    Entry finder;
    finder.name = path;
    return _entries.find(finder)->size_uncompressed;
}

Source& ZipFileMinizImpl::read(const Path &path) {
    Entry finder;
    finder.name = path;
    Source *source = new SourceImpl(&_zip, _entries.find(finder)->file_index);
    _sources.push_back(source);
    return *source;
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

ZipFileMinizImpl::SourceImpl::SourceImpl(mz_zip_archive *zip, mz_uint file_index) {
    _reader = mz_zip_reader_extract_iter_new(zip, file_index, 0);
}

std::size_t ZipFileMinizImpl::SourceImpl::read(Byte *buffer, std::size_t size) const {
    if (_reader == nullptr) {
        return 0;
    }
    return mz_zip_reader_extract_iter_read(_reader, buffer, size);
}

void ZipFileMinizImpl::SourceImpl::close() {
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

ZipFile::ConstIterator ZipFile::begin() const {
    return ConstIterator(begin_());
}

ZipFile::ConstIterator ZipFile::end() const {
    return ConstIterator(end_());
}

ZipFile::ConstIterator::ConstIterator(std::unique_ptr<ZipFile::ConstIterator_> it) :
    _it(std::move(it)) {
}

ZipFile::ConstIterator::ConstIterator(const ZipFile::ConstIterator &other) :
    _it(other._it->copy()) {
}

const ZipFile::Entry& ZipFile::ConstIterator::operator*() const {
    return _it->operator*();
}

ZipFile::ConstIterator& ZipFile::ConstIterator::operator++() {
    _it->operator++();
    return *this;
}

bool ZipFile::ConstIterator::operator!=(const ZipFile::ConstIterator &it) const {
    return _it->operator!=(*it._it);
}

}
