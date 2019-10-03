#include "ZipStorage.h"
#include "miniz.h"
#include "Path.h"
#include "Stream.h"

namespace odr {

class ZipReader::Impl final {
public:
    mz_zip_archive zip;
    mz_zip_archive_file_stat tmp_stat;

    class SourceImpl final : public Source {
    public:
        mz_zip_reader_extract_iter_state *iter;
        std::uint64_t remaining;

        explicit SourceImpl(mz_zip_reader_extract_iter_state *iter) :
                iter(iter),
                remaining(iter->file_stat.m_uncomp_size) {
        }

        ~SourceImpl() final {
            mz_zip_reader_extract_iter_free(iter);
        }

        std::uint32_t read(char *data, std::uint32_t amount) final {
            if (remaining <= 0) return 0;
            if (remaining < amount) amount = remaining;
            const std::uint32_t result = mz_zip_reader_extract_iter_read(iter, data, amount);
            remaining -= result;
            return result;
        }

        std::uint32_t available() const final {
            return remaining;
        }
    };

    explicit Impl(const std::string &path) {
        memset(&zip, 0, sizeof(zip));
        const mz_bool status = mz_zip_reader_init_file(&zip, path.data(), MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
        if (!status) {
            throw; // TODO
        }
    }

    ~Impl() {
        mz_zip_reader_end(&zip);
    }

    bool stat(const std::string &path, mz_zip_archive_file_stat &result) {
        mz_uint i;
        if (!find(path, i)) {
            return false;
        }
        return mz_zip_reader_file_stat(&zip, i, &result);
    }

    bool find(const std::string &path, mz_uint &i) {
        int tmp = mz_zip_reader_locate_file(&zip, path.data(), nullptr, 0);
        if (tmp < 0) {
            return false;
        }
        i = tmp;
        return true;
    }

    bool isSomething(const Path &path) {
        mz_uint dummy;
        return find(path, dummy);
    }

    bool isFile(const Path &path) {
        mz_uint i;
        return find(path, i) && !mz_zip_reader_is_file_a_directory(&zip, i);
    }

    bool isFolder(const Path &path) {
        mz_uint i;
        return find(path, i) && mz_zip_reader_is_file_a_directory(&zip, i);
    }

    bool isReadable(const Path &path) {
        return isFile(path);
    }

    std::uint64_t size(const Path &path) {
        if (!stat(path, tmp_stat)) {
            return false;
        }
        return tmp_stat.m_uncomp_size;
    }

    void visit(Visiter visiter) {
        for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip); ++i) {
            mz_zip_reader_get_filename(&zip, i, tmp_stat.m_filename, sizeof(impl->tmp_stat.m_filename));
            visiter(Path(tmp_stat.m_filename));
        }
    }

    void visit(const Path &path, Visiter visiter) {
        if (!isFolder(path)) {
            return;
        }
        visit([&](const auto &p) { if (p.childOf(path)) visiter(p); });
    }

    std::unique_ptr<Source> read(const Path &path) {
        auto iter = mz_zip_reader_extract_file_iter_new(&zip, path.string().c_str(), 0);
        if (iter == nullptr) {
            return nullptr;
        }
        return std::make_unique<SourceImpl>(iter);
    }
};

class ZipWriter::Impl final {
public:
    mz_zip_archive zip;

    class SinkImpl final : public Sink {
    public:
        mz_zip_archive &zip;
        const std::string path;
        std::string buffer;

        SinkImpl(mz_zip_archive &zip, const std::string &path) :
                zip(zip),
                path(path) {
        }

        ~SinkImpl() final {
            mz_zip_writer_add_mem(&zip, path.data(), buffer.data(), buffer.size(), MZ_DEFAULT_COMPRESSION);
        }

        void write(const char *data, const std::uint32_t amount) final {
            // TODO this is super inefficient
            buffer += std::string(data, amount);
        }
    };

    explicit Impl(const std::string &path) {
        memset(&zip, 0, sizeof(zip));
        const mz_bool status = mz_zip_writer_init_file(&zip, path.data(), 0);
        if (!status) {
            throw; // TODO
        }
    }

    ~Impl() {
        mz_zip_writer_finalize_archive(&zip);
        mz_zip_writer_end(&zip);
    }

    bool copy(const ZipReader &source, const Path &path) {
        mz_uint i;
        if (!source.impl->find(path, i)) {
            return false;
        }
        mz_zip_writer_add_from_zip_reader(&zip, &source.impl->zip, i);
        return true;
    }

    std::unique_ptr<Sink> write(const Path &path) {
        return std::make_unique<SinkImpl>(zip, path);
    }
};

ZipReader::ZipReader(const Path &path) :
        impl(std::make_unique<Impl>(path)) {
}

ZipReader::~ZipReader() = default;

bool ZipReader::isSomething(const Path &path) const {
    return impl->isSomething(path);
}

bool ZipReader::isFile(const Path &path) const {
    return impl->isFile(path);
}

bool ZipReader::isFolder(const Path &path) const {
    return impl->isFolder(path);
}

bool ZipReader::isReadable(const Path &path) const {
    return impl->isReadable(path);
}

std::uint64_t ZipReader::size(const Path &path) const {
    return impl->size(path);
}

void ZipReader::visit(Visiter visiter) const {
    return impl->visit(visiter);
}

void ZipReader::visit(const Path &path, Visiter visiter) const {
    return impl->visit(path, visiter);
}

std::unique_ptr<Source> ZipReader::read(const Path &path) const {
    return impl->read(path);
}

ZipWriter::ZipWriter(const Path &path) :
        impl(std::make_unique<Impl>(path)) {
}

ZipWriter::~ZipWriter() = default;

bool ZipWriter::copy(const ZipReader &source, const Path &path) const {
    return impl->copy(source, path);
}

std::unique_ptr<Sink> ZipWriter::write(const Path &path) const {
    return impl->write(path);
}

}
