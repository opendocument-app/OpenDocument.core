#include <odr/internal/zip/zip_util.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/temporary_file.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <limits>
#include <optional>
#include <system_error>
#include <utility>

namespace odr::internal::zip::util {

namespace {

class ReaderBuffer final : public std::streambuf {
public:
  ReaderBuffer(std::shared_ptr<const Archive> archive,
               const std::uint32_t index)
      : m_archive{std::move(archive)}, m_zip{*m_archive->zip()},
        m_iter{mz_zip_reader_extract_iter_new(&m_zip, index, 0),
               mz_zip_reader_extract_iter_free} {
    if (m_iter == nullptr) {
      throw FileReadError();
    }
    m_remaining = m_iter->file_stat.m_uncomp_size;
    if (m_remaining == 0) {
      finish();
    }
  }

protected:
  int underflow() override {
    if (m_remaining == 0) {
      return traits_type::eof();
    }

    const std::uint64_t amount =
        std::min<std::uint64_t>(m_remaining, m_buffer.size());
    const std::size_t result =
        mz_zip_reader_extract_iter_read(m_iter.get(), m_buffer.data(), amount);
    if (result != amount) {
      throw FileReadError();
    }
    m_remaining -= result;
    if (m_remaining == 0) {
      finish();
    }
    setg(m_buffer.data(), m_buffer.data(), m_buffer.data() + result);

    return traits_type::to_int_type(*gptr());
  }

private:
  void finish() {
    // Finish inflation and validate size/CRC before exposing the final bytes.
    char extra{};
    const auto excess =
        mz_zip_reader_extract_iter_read(m_iter.get(), &extra, 1);
    const bool valid = mz_zip_reader_extract_iter_free(m_iter.release());
    if (excess != 0 || !valid) {
      throw FileReadError();
    }
  }

  std::shared_ptr<const Archive> m_archive;
  mz_zip_archive m_zip;
  std::unique_ptr<mz_zip_reader_extract_iter_state,
                  decltype(&mz_zip_reader_extract_iter_free)>
      m_iter;
  std::uint64_t m_remaining{0};
  std::array<char, 4096> m_buffer{};
};

class FileInZipIstream final : public std::istream {
public:
  explicit FileInZipIstream(std::unique_ptr<ReaderBuffer> sbuf)
      : std::istream(sbuf.get()), m_sbuf{std::move(sbuf)} {
    if (m_sbuf == nullptr) {
      throw NullPointerError("FileInZipIstream: sbuf is nullptr");
    }
  }

private:
  std::unique_ptr<ReaderBuffer> m_sbuf;
};

class FileInZip final : public abstract::File {
public:
  FileInZip(std::shared_ptr<const Archive> archive, const std::uint32_t index,
            std::string name)
      : m_archive{std::move(archive)}, m_index{index}, m_name{std::move(name)} {
    if (m_archive == nullptr) {
      throw NullPointerError("FileInZip: archive is nullptr");
    }
  }

  [[nodiscard]] FileLocation location() const noexcept override {
    return m_archive->file()->location();
  }
  [[nodiscard]] std::size_t size() const override {
    auto zip = *m_archive->zip();
    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(&zip, m_index, &stat) ||
        !std::in_range<std::size_t>(stat.m_uncomp_size)) {
      throw FileReadError();
    }
    return static_cast<std::size_t>(stat.m_uncomp_size);
  }

  [[nodiscard]] std::string name() const override { return m_name; }

  [[nodiscard]] std::optional<AbsPath> disk_path() const override {
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string_view> memory_data() const override {
    return std::nullopt;
  }

  [[nodiscard]] std::unique_ptr<std::istream> stream() const override {
    auto zip = *m_archive->zip();
    if (mz_zip_reader_is_file_encrypted(&zip, m_index)) {
      throw UnsupportedOperation("cannot read encrypted zip entry");
    }
    if (!mz_zip_reader_is_file_supported(&zip, m_index)) {
      throw UnsupportedOperation("zip entry not supported");
    }
    return std::make_unique<FileInZipIstream>(
        std::make_unique<ReaderBuffer>(m_archive, m_index));
  }

private:
  std::shared_ptr<const Archive> m_archive;
  std::uint32_t m_index;
  std::string m_name;
};

} // namespace

bool Archive::Entry::is_file() const { return !is_directory(); }

bool Archive::Entry::is_directory() const {
  auto zip = *m_archive->zip();
  return mz_zip_reader_is_file_a_directory(&zip, m_index);
}

RelPath Archive::Entry::path() const {
  auto zip = *m_archive->zip();
  const mz_uint size = mz_zip_reader_get_filename(&zip, m_index, nullptr, 0);
  if (size == 0) {
    throw FileReadError();
  }
  std::string filename(size, '\0');
  if (mz_zip_reader_get_filename(&zip, m_index, filename.data(), size) !=
      size) {
    throw FileReadError();
  }
  filename.pop_back();
  if (filename.find('\0') != std::string::npos) {
    throw FileReadError();
  }
  // APPNOTE.TXT 4.4.17.1 forbids a leading slash; tolerate it on read.
  return Path(filename).make_relative();
}

Method Archive::Entry::method() const {
  auto zip = *m_archive->zip();
  mz_zip_archive_file_stat stat{};
  if (!mz_zip_reader_file_stat(&zip, m_index, &stat)) {
    throw FileReadError();
  }
  switch (stat.m_method) {
  case 0:
    return Method::STORED;
  case MZ_DEFLATED:
    return Method::DEFLATED;
  default:
    return Method::UNSUPPORTED;
  }
}

std::shared_ptr<abstract::File> Archive::Entry::file() const {
  if (!is_file()) {
    return nullptr;
  }
  return std::make_shared<FileInZip>(m_archive->shared_from_this(), m_index,
                                     path().basename());
}

ReadSource::ReadSource(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  if (m_file == nullptr) {
    throw NullPointerError("ReadSource: file is nullptr");
  }
  m_memory = m_file->memory_data();
}

void ReadSource::release_source(const AbsPath &path) {
  const std::lock_guard lock(m_mutex);
  const std::optional<AbsPath> source = m_file->disk_path();
  // false where either file is missing, such as a source deleted after open
  std::error_code error;
  if (!source.has_value() ||
      !std::filesystem::equivalent(source->path(), path.path(), error)) {
    return;
  }
  m_file = std::make_shared<TemporaryDiskFile>(
      TemporaryDiskFileFactory::system_default().copy(*m_file));
  m_streams.clear();
}

std::size_t ReadSource::read(const std::uint64_t offset, void *buffer,
                             const std::size_t size) const {
  if (m_memory.has_value()) {
    if (offset >= m_memory->size()) {
      return 0;
    }
    const std::size_t amount =
        std::min<std::size_t>(size, m_memory->size() - offset);
    std::memcpy(buffer, m_memory->data() + offset, amount);
    return amount;
  }

  if (!std::in_range<std::streamoff>(offset) ||
      !std::in_range<std::streamsize>(size)) {
    return 0;
  }
  std::unique_ptr<std::istream> stream;
  std::shared_ptr<abstract::File> source;
  {
    std::lock_guard lock(m_mutex);
    source = m_file;
    if (!m_streams.empty()) {
      stream = std::move(m_streams.back());
      m_streams.pop_back();
    } else {
      stream = source->stream();
    }
  }

  // A short read has to surface as one. Clear first so an earlier read past the
  // end does not poison every later seek.
  stream->clear();
  stream->seekg(static_cast<std::streamoff>(offset));
  stream->read(static_cast<char *>(buffer), static_cast<std::streamsize>(size));
  const auto result = static_cast<std::size_t>(stream->gcount());

  {
    std::lock_guard lock(m_mutex);
    if (source == m_file) {
      m_streams.push_back(std::move(stream));
    }
  }

  return result;
}

Archive::Archive(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  if (m_file == nullptr) {
    throw NullPointerError("Archive: file is nullptr");
  }
  m_source = std::make_unique<ReadSource>(m_file);
  try {
    open_from_file(m_zip, *m_file, *m_source);
  } catch (...) {
    mz_zip_end(&m_zip);
    throw;
  }
}

Archive::~Archive() { mz_zip_end(&m_zip); }

void Archive::release_source(const AbsPath &path) const {
  m_source->release_source(path);
}

mz_zip_archive *Archive::zip() const { return &m_zip; }

std::shared_ptr<abstract::File> Archive::file() const noexcept {
  return m_file;
}

Archive::Iterator Archive::begin() const { return {*this, 0}; }

Archive::Iterator Archive::end() const {
  return {*this, mz_zip_reader_get_num_files(&m_zip)};
}

Archive::Iterator Archive::find(const RelPath &path) const {
  return std::find_if(begin(), end(), [&path](const Entry &entry) {
    return entry.path() == path;
  });
}

} // namespace odr::internal::zip::util

namespace odr::internal::zip {

void util::open_from_file(mz_zip_archive &archive, const abstract::File &file,
                          ReadSource &source) {
  archive.m_pIO_opaque = &source;
  archive.m_pRead = [](void *opaque, const std::uint64_t offset, void *buffer,
                       const std::size_t size) {
    return static_cast<const ReadSource *>(opaque)->read(offset, buffer, size);
  };
  const bool state = mz_zip_reader_init(
      &archive, file.size(), MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
  if (!state) {
    throw NoZipFile();
  }
}

bool util::append_file(mz_zip_archive &archive, const std::string &path,
                       std::istream &istream, const std::size_t size,
                       const std::time_t &time, const std::string &comment,
                       const std::uint32_t level_and_flags) {
  auto read_callback = [](void *opaque, std::uint64_t /*offset*/, void *buffer,
                          const std::size_t s) -> std::size_t {
    const auto in = static_cast<std::istream *>(opaque);
    if (!std::in_range<std::streamsize>(s)) {
      return 0;
    }
    in->read(static_cast<char *>(buffer), static_cast<std::streamsize>(s));
    return in->bad() ? 0 : static_cast<std::size_t>(in->gcount());
  };

  // Without the flag the size only lands in a trailing data descriptor, which
  // LibreOffice rejects on a stored entry.
  return mz_zip_writer_add_read_buf_callback(
      &archive, path.c_str(), read_callback, &istream, size, &time,
      comment.c_str(), comment.size(),
      level_and_flags | MZ_ZIP_FLAG_WRITE_HEADER_SET_SIZE, "", 0, "", 0);
}

} // namespace odr::internal::zip
