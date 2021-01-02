#include <common/path.h>
#include <miniz.h>
#include <odr/exceptions.h>
#include <sstream>
#include <streambuf>
#include <utility>
#include <zip/zip_storage.h>

namespace odr::zip {

namespace {
constexpr std::uint64_t buffer_size_ = 4098;

class ZipReaderBuf final : public std::streambuf {
public:
  explicit ZipReaderBuf(mz_zip_reader_extract_iter_state *iter)
      : iter_(iter), remaining_(iter->file_stat.m_uncomp_size),
        buffer_(new char[buffer_size_]) {}

  ~ZipReaderBuf() final {
    mz_zip_reader_extract_iter_free(iter_);
    delete[] buffer_;
  }

  int underflow() final {
    if (remaining_ <= 0)
      return std::char_traits<char>::eof();

    const std::uint64_t amount = std::min(remaining_, buffer_size_);
    const std::uint32_t result =
        mz_zip_reader_extract_iter_read(iter_, buffer_, amount);
    remaining_ -= result;
    this->setg(this->buffer_, this->buffer_, this->buffer_ + result);

    return std::char_traits<char>::to_int_type(*gptr());
  }

private:
  mz_zip_reader_extract_iter_state *iter_;
  std::uint64_t remaining_;
  char *buffer_;
};

class ZipWriterBuf final : public std::stringbuf {
public:
  ZipWriterBuf(mz_zip_archive &zip, std::string path, int compression)
      : zip_(zip), path_(std::move(path)), compression_(compression) {}
  ~ZipWriterBuf() final {
    // TODO this is super inefficient
    const auto buffer = str();
    mz_zip_writer_add_mem(&zip_, path_.data(), buffer.data(), buffer.size(),
                          compression_);
  }

private:
  mz_zip_archive &zip_;
  const std::string path_;
  const int compression_;
};

class ZipReaderIstream final : public std::istream {
public:
  explicit ZipReaderIstream(mz_zip_reader_extract_iter_state *iter)
      : ZipReaderIstream(new ZipReaderBuf(iter)) {}
  explicit ZipReaderIstream(ZipReaderBuf *sbuf)
      : std::istream(sbuf), sbuf_(sbuf) {}
  ~ZipReaderIstream() final { delete sbuf_; }

private:
  ZipReaderBuf *sbuf_;
};

class ZipWriterOstream final : public std::ostream {
public:
  ZipWriterOstream(mz_zip_archive &zip, std::string path, const int compression)
      : ZipWriterOstream(new ZipWriterBuf(zip, std::move(path), compression)) {}
  explicit ZipWriterOstream(ZipWriterBuf *sbuf)
      : std::ostream(sbuf), sbuf_(sbuf) {}
  ~ZipWriterOstream() final { delete sbuf_; }

private:
  ZipWriterBuf *sbuf_;
};
} // namespace

class ZipReader::Impl final {
public:
  Impl(const void *mem, const std::uint64_t size) {
    memset(&zip, 0, sizeof(zip));
    const mz_bool status = mz_zip_reader_init_mem(
        &zip, mem, size, MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
    if (!status)
      throw NoZipFile();
  }

  explicit Impl(std::string data) : buffer(std::move(data)) {
    memset(&zip, 0, sizeof(zip));
    const mz_bool status =
        mz_zip_reader_init_mem(&zip, buffer.data(), buffer.size(),
                               MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
    if (!status)
      throw NoZipFile();
  }

  explicit Impl(const common::Path &path) {
    memset(&zip, 0, sizeof(zip));
    const mz_bool status = mz_zip_reader_init_file(
        &zip, path.string().data(), MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
    if (!status)
      throw NoZipFile();
  }

  ~Impl() { mz_zip_reader_end(&zip); }

  bool stat(const std::string &path,
            mz_zip_archive_file_stat &result) noexcept {
    mz_uint i;
    if (!find(path, i))
      return false;
    return mz_zip_reader_file_stat(&zip, i, &result);
  }

  bool find(const std::string &path, mz_uint &i) noexcept {
    int tmp = mz_zip_reader_locate_file(&zip, path.data(), nullptr, 0);
    if (tmp < 0)
      return false;
    i = tmp;
    return true;
  }

  bool isSomething(const common::Path &path) noexcept {
    mz_uint dummy;
    return find(path, dummy);
  }

  bool isFile(const common::Path &path) noexcept {
    mz_uint i;
    return find(path, i) && !mz_zip_reader_is_file_a_directory(&zip, i);
  }

  bool isDirectory(const common::Path &path) noexcept {
    mz_uint i;
    return find(path.string() + "/", i) &&
           mz_zip_reader_is_file_a_directory(&zip, i);
  }

  bool isReadable(const common::Path &path) noexcept { return isFile(path); }

  std::uint64_t size(const common::Path &path) noexcept {
    if (!stat(path, tmp_stat))
      return false;
    return tmp_stat.m_uncomp_size;
  }

  void visit(Visitor visitor) {
    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip); ++i) {
      mz_zip_reader_get_filename(&zip, i, tmp_stat.m_filename,
                                 sizeof(impl->tmp_stat.m_filename));
      visitor(common::Path(tmp_stat.m_filename));
    }
  }

  std::unique_ptr<std::istream> read(const common::Path &path) noexcept {
    auto iter =
        mz_zip_reader_extract_file_iter_new(&zip, path.string().c_str(), 0);
    if (iter == nullptr)
      return nullptr;
    return std::make_unique<ZipReaderIstream>(iter);
  }

  // private:
  std::string buffer;
  mz_zip_archive zip{};
  mz_zip_archive_file_stat tmp_stat{};
};

class ZipWriter::Impl final {
public:
  explicit Impl(const std::string &path) {
    memset(&zip, 0, sizeof(zip));
    const mz_bool status = mz_zip_writer_init_file(&zip, path.data(), 0);
    if (!status)
      throw common::FileNotCreatedException(path);
  }

  ~Impl() {
    mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);
  }

  bool copy(const ZipReader &source, const common::Path &path) noexcept {
    mz_uint i;
    if (!source.impl->find(path, i))
      return false;
    mz_zip_writer_add_from_zip_reader(&zip, &source.impl->zip, i);
    return true;
  }

  bool createDirectory(const common::Path &path) noexcept {
    const std::string dir = path.string() + "/";
    mz_zip_writer_add_mem(&zip, dir.data(), "", 0, 0);
    return true;
  }

  std::unique_ptr<std::ostream> write(const common::Path &path,
                                      const int compression) noexcept {
    return std::make_unique<ZipWriterOstream>(zip, path.string(), compression);
  }

public:
  mz_zip_archive zip{};
};

ZipReader::ZipReader(const void *mem, const std::uint64_t size)
    : impl(std::make_unique<Impl>(mem, size)) {}

ZipReader::ZipReader(const std::string &data, bool)
    : impl(std::make_unique<Impl>(data)) {}

ZipReader::ZipReader(const common::Path &path)
    : impl(std::make_unique<Impl>(path)) {}

ZipReader::~ZipReader() = default;

bool ZipReader::isSomething(const common::Path &path) const {
  return impl->isSomething(path);
}

bool ZipReader::isFile(const common::Path &path) const {
  return impl->isFile(path);
}

bool ZipReader::isDirectory(const common::Path &path) const {
  return impl->isDirectory(path);
}

bool ZipReader::isReadable(const common::Path &path) const {
  return impl->isReadable(path);
}

std::uint64_t ZipReader::size(const common::Path &path) const {
  return impl->size(path);
}

void ZipReader::visit(Visitor visitor) const { return impl->visit(visitor); }

std::unique_ptr<std::istream> ZipReader::read(const common::Path &path) const {
  return impl->read(path);
}

ZipWriter::ZipWriter(const common::Path &path)
    : impl(std::make_unique<Impl>(path)) {}

ZipWriter::~ZipWriter() = default;

bool ZipWriter::copy(const ZipReader &source, const common::Path &path) const {
  return impl->copy(source, path);
}

bool ZipWriter::createDirectory(const common::Path &path) const {
  return impl->createDirectory(path);
}

std::unique_ptr<std::ostream> ZipWriter::write(const common::Path &path) const {
  return impl->write(path, MZ_DEFAULT_LEVEL);
}

std::unique_ptr<std::ostream> ZipWriter::write(const common::Path &path,
                                               const int compression) const {
  return impl->write(path, compression);
}

} // namespace odr::zip
