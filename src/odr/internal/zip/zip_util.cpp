#include <odr/internal/zip/zip_util.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/common/file.hpp>

#include <algorithm>
#include <utility>

namespace odr::internal::zip::util {

namespace {

class ReaderBuffer final : public std::streambuf {
public:
  explicit ReaderBuffer(mz_zip_reader_extract_iter_state *iter);
  ReaderBuffer(mz_zip_reader_extract_iter_state *iter, std::size_t buffer_size);
  ~ReaderBuffer() final;

  int underflow() final;

private:
  mz_zip_reader_extract_iter_state *m_iter{};
  std::uint64_t m_remaining{0};
  std::uint64_t m_buffer_size{4098};
  char *m_buffer;
};

class FileInZipIstream final : public std::istream {
public:
  FileInZipIstream(std::shared_ptr<Archive> archive,
                   std::unique_ptr<ReaderBuffer> sbuf);
  FileInZipIstream(std::shared_ptr<Archive> archive,
                   mz_zip_reader_extract_iter_state *iter);

private:
  std::shared_ptr<Archive> m_archive;
  std::unique_ptr<ReaderBuffer> m_sbuf;
};

ReaderBuffer::ReaderBuffer(mz_zip_reader_extract_iter_state *iter)
    : ReaderBuffer(iter, 4098) {}

ReaderBuffer::ReaderBuffer(mz_zip_reader_extract_iter_state *iter,
                           const std::size_t buffer_size)
    : m_iter{iter}, m_remaining{iter->file_stat.m_uncomp_size},
      m_buffer_size{buffer_size}, m_buffer{new char[m_buffer_size]} {}

ReaderBuffer::~ReaderBuffer() {
  mz_zip_reader_extract_iter_free(m_iter);
  delete[] m_buffer;
}

int ReaderBuffer::underflow() {
  if (m_remaining <= 0) {
    return std::char_traits<char>::eof();
  }

  const std::uint64_t amount = std::min(m_remaining, m_buffer_size);
  const std::uint32_t result =
      mz_zip_reader_extract_iter_read(m_iter, m_buffer, amount);
  m_remaining -= result;
  setg(m_buffer, m_buffer, m_buffer + result);

  return std::char_traits<char>::to_int_type(*gptr());
}

FileInZipIstream::FileInZipIstream(std::shared_ptr<Archive> archive,
                                   std::unique_ptr<ReaderBuffer> sbuf)
    : std::istream(sbuf.get()), m_archive{std::move(archive)}, m_sbuf{std::move(
                                                                   sbuf)} {}

FileInZipIstream::FileInZipIstream(std::shared_ptr<Archive> archive,
                                   mz_zip_reader_extract_iter_state *iter)
    : FileInZipIstream(std::move(archive),
                       std::make_unique<ReaderBuffer>(iter)) {}

} // namespace

Archive::Archive(const std::shared_ptr<common::MemoryFile> &file)
    : Archive(std::dynamic_pointer_cast<abstract::File>(file)) {}

Archive::Archive(const std::shared_ptr<common::DiskFile> &file)
    : Archive(std::dynamic_pointer_cast<abstract::File>(file)) {}

Archive::Archive(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)}, m_data{m_file->stream()} {
  init_();
}

Archive::Archive(const Archive &other) : Archive(other.m_file) {}

Archive::Archive(Archive &&other) noexcept = default;

Archive::~Archive() { mz_zip_end(&m_zip); }

Archive &Archive::operator=(const Archive &other) {
  if (&other != this) {
    m_zip = other.m_zip;
    m_file = other.m_file;
    m_data = m_file->stream();
    init_();
  }
  return *this;
}

Archive &Archive::operator=(Archive &&) noexcept = default;

void Archive::init_() {
  m_zip.m_pIO_opaque = m_data.get();
  m_zip.m_pRead = [](void *opaque, std::uint64_t offset, void *buffer,
                     std::size_t size) {
    auto in = static_cast<std::istream *>(opaque);
    in->seekg(offset);
    in->read(static_cast<char *>(buffer), size);
    return size;
  };
  const bool state = mz_zip_reader_init(
      &m_zip, m_file->size(), MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
  if (!state) {
    throw NoZipFile();
  }
}

mz_zip_archive *Archive::zip() const { return &m_zip; }

std::shared_ptr<abstract::File> Archive::file() const { return m_file; }

FileInZip::FileInZip(std::shared_ptr<Archive> archive,
                     const std::uint32_t index)
    : m_archive{std::move(archive)}, m_index{index} {}

FileLocation FileInZip::location() const noexcept {
  return m_archive->file()->location();
}

std::size_t FileInZip::size() const {
  mz_zip_archive_file_stat stat{};
  mz_zip_reader_file_stat(m_archive->zip(), m_index, &stat);
  return stat.m_uncomp_size;
}

std::optional<common::Path> FileInZip::disk_path() const { return {}; }

const char *FileInZip::memory_data() const { return nullptr; }

std::unique_ptr<std::istream> FileInZip::stream() const {
  auto iter = mz_zip_reader_extract_iter_new(m_archive->zip(), m_index, 0);
  return std::make_unique<FileInZipIstream>(m_archive, iter);
}

bool append_file(mz_zip_archive &archive, const std::string &path,
                 std::istream &istream, const std::size_t size,
                 const std::time_t &time, const std::string &comment,
                 const std::uint32_t level_and_flags) {
  auto read_callback = [](void *opaque, std::uint64_t /*offset*/, void *buffer,
                          std::size_t size) {
    auto istream = static_cast<std::istream *>(opaque);
    istream->read(static_cast<char *>(buffer), size);
    return size;
  };

  return mz_zip_writer_add_read_buf_callback(
      &archive, path.c_str(), read_callback, &istream, size, &time,
      comment.c_str(), comment.size(), level_and_flags, nullptr, 0, nullptr, 0);
}

} // namespace odr::internal::zip::util
