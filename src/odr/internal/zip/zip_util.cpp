#include <odr/internal/zip/zip_util.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/common/file.hpp>

#include <algorithm>

namespace odr::internal::zip::util {

namespace {

class ReaderBuffer final : public std::streambuf {
public:
  ReaderBuffer(std::shared_ptr<const Archive> archive,
               mz_zip_reader_extract_iter_state *iter, std::size_t buffer_size)
      : m_archive{std::move(archive)} {
    if (m_archive == nullptr) {
      throw NullPointerError("ReaderBuffer: archive is nullptr");
    }
    if (iter == nullptr) {
      throw NullPointerError("ReaderBuffer: iter is nullptr");
    }
    m_iter = iter;
    m_remaining = iter->file_stat.m_uncomp_size;
    m_buffer_size = buffer_size;
    m_buffer = new char[m_buffer_size];
  }
  ReaderBuffer(const ReaderBuffer &) = delete;
  ReaderBuffer(ReaderBuffer &&other) noexcept
      : m_iter{other.m_iter}, m_remaining{other.m_remaining},
        m_buffer_size{other.m_buffer_size}, m_buffer{other.m_buffer} {
    other.m_iter = nullptr;
    other.m_buffer = nullptr;
  }
  ~ReaderBuffer() final {
    mz_zip_reader_extract_iter_free(m_iter);
    delete[] m_buffer;
  }

  ReaderBuffer &operator=(const ReaderBuffer &) = delete;
  ReaderBuffer &operator=(ReaderBuffer &&other) noexcept {
    if (&other != this) {
      m_iter = other.m_iter;
      m_remaining = other.m_remaining;
      m_buffer_size = other.m_buffer_size;
      m_buffer = other.m_buffer;
      other.m_iter = nullptr;
      other.m_buffer = nullptr;
    }
    return *this;
  }

  int underflow() final {
    if (m_remaining <= 0) {
      return std::char_traits<char>::eof();
    }

    std::lock_guard lock(m_archive->mutex());

    const std::uint64_t amount = std::min(m_remaining, m_buffer_size);
    const std::uint32_t result =
        mz_zip_reader_extract_iter_read(m_iter, m_buffer, amount);
    m_remaining -= result;
    setg(m_buffer, m_buffer, m_buffer + result);

    return std::char_traits<char>::to_int_type(*gptr());
  }

private:
  std::shared_ptr<const Archive> m_archive;
  mz_zip_reader_extract_iter_state *m_iter{};
  std::uint64_t m_remaining{0};
  std::uint64_t m_buffer_size{4098};
  char *m_buffer;
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
  FileInZip(std::shared_ptr<const Archive> archive, std::uint32_t index)
      : m_archive{std::move(archive)}, m_index{index} {
    if (m_archive == nullptr) {
      throw NullPointerError("FileInZip: archive is nullptr");
    }
  }

  [[nodiscard]] FileLocation location() const noexcept final {
    return m_archive->file()->location();
  }
  [[nodiscard]] std::size_t size() const final {
    std::lock_guard lock(m_archive->mutex());
    mz_zip_archive_file_stat stat{};
    mz_zip_reader_file_stat(m_archive->zip(), m_index, &stat);
    return stat.m_uncomp_size;
  }

  [[nodiscard]] std::optional<AbsPath> disk_path() const final {
    return std::nullopt;
  }
  [[nodiscard]] const char *memory_data() const final { return nullptr; }

  [[nodiscard]] std::unique_ptr<std::istream> stream() const final {
    std::lock_guard lock(m_archive->mutex());
    if (mz_zip_reader_is_file_encrypted(m_archive->zip(), m_index)) {
      throw UnsupportedOperation("cannot read encrypted zip entry");
    }
    if (!mz_zip_reader_is_file_supported(m_archive->zip(), m_index)) {
      throw UnsupportedOperation("zip entry not supported");
    }
    auto iter = mz_zip_reader_extract_iter_new(m_archive->zip(), m_index, 0);
    if (iter == nullptr) {
      throw FileNotFound("zip entry not found " + std::to_string(m_index));
    }
    return std::make_unique<FileInZipIstream>(
        std::make_unique<ReaderBuffer>(m_archive, iter, 4098));
  }

private:
  std::shared_ptr<const Archive> m_archive;
  std::uint32_t m_index;
};

} // namespace

bool Archive::Entry::is_file() const {
  std::lock_guard lock(m_archive->mutex());
  return !mz_zip_reader_is_file_a_directory(m_archive->zip(), m_index);
}

bool Archive::Entry::is_directory() const {
  std::lock_guard lock(m_archive->mutex());
  return mz_zip_reader_is_file_a_directory(m_archive->zip(), m_index);
}

Path Archive::Entry::path() const {
  std::lock_guard lock(m_archive->mutex());
  char filename[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];
  mz_zip_reader_get_filename(m_archive->zip(), m_index, filename,
                             MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE);
  return Path(filename).make_absolute();
}

Method Archive::Entry::method() const {
  std::lock_guard lock(m_archive->mutex());
  mz_zip_archive_file_stat stat{};
  mz_zip_reader_file_stat(m_archive->zip(), m_index, &stat);
  switch (stat.m_method) {
  case 0:
    return Method::STORED;
  case MZ_DEFLATED:
    return Method::DEFLATED;
  }
  return Method::UNSUPPORTED;
}

std::shared_ptr<abstract::File> Archive::Entry::file() const {
  if (!is_file()) {
    return nullptr;
  }
  return std::make_shared<FileInZip>(m_archive->shared_from_this(), m_index);
}

Archive::Archive(const std::shared_ptr<MemoryFile> &file)
    : Archive(std::dynamic_pointer_cast<abstract::File>(file)) {}

Archive::Archive(const std::shared_ptr<DiskFile> &file)
    : Archive(std::dynamic_pointer_cast<abstract::File>(file)) {}

Archive::Archive(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  if (m_file == nullptr) {
    throw NullPointerError("Archive: file is nullptr");
  }
  m_stream = m_file->stream();
  open_from_file(m_zip, *m_file, *m_stream);
}

Archive::~Archive() {
  std::lock_guard lock(m_mutex);
  mz_zip_end(&m_zip);
}

std::mutex &Archive::mutex() const { return m_mutex; }

mz_zip_archive *Archive::zip() const { return &m_zip; }

std::shared_ptr<abstract::File> Archive::file() const noexcept {
  return m_file;
}

Archive::Iterator Archive::begin() const { return {*this, 0}; }

Archive::Iterator Archive::end() const {
  std::lock_guard lock(m_mutex);
  return {*this, mz_zip_reader_get_num_files(&m_zip)};
}

Archive::Iterator Archive::find(const Path &path) const {
  return std::find_if(begin(), end(), [&path](const Entry &entry) {
    return entry.path() == path;
  });
}

} // namespace odr::internal::zip::util

namespace odr::internal::zip {

void util::open_from_file(mz_zip_archive &archive, const abstract::File &file,
                          std::istream &stream) {
  archive.m_pIO_opaque = &stream;
  archive.m_pRead = [](void *opaque, std::uint64_t offset, void *buffer,
                       std::size_t size) {
    auto in = static_cast<std::istream *>(opaque);
    in->seekg(offset);
    in->read(static_cast<char *>(buffer), size);
    return size;
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
                          std::size_t size) -> std::size_t {
    auto istream = static_cast<std::istream *>(opaque);
    istream->read(static_cast<char *>(buffer), size);
    return istream->gcount();
  };

  return mz_zip_writer_add_read_buf_callback(
      &archive, path.c_str(), read_callback, &istream, size, &time,
      comment.c_str(), comment.size(), level_and_flags, "", 0, "", 0);
}

} // namespace odr::internal::zip
