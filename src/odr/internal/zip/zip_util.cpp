#include <odr/internal/zip/zip_util.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/common/file.hpp>

#include <algorithm>
#include <array>
#include <cstring>

namespace odr::internal::zip::util {

namespace {

class ReaderBuffer final : public std::streambuf {
public:
  ReaderBuffer(std::shared_ptr<const Archive> archive,
               mz_zip_reader_extract_iter_state *iter,
               const std::size_t buffer_size = 4096)
      : m_archive{std::move(archive)}, m_buffer(buffer_size, '\0') {
    if (m_archive == nullptr) {
      throw NullPointerError("ReaderBuffer: archive is nullptr");
    }
    if (iter == nullptr) {
      throw NullPointerError("ReaderBuffer: iter is nullptr");
    }
    m_iter = iter;
    m_remaining = iter->file_stat.m_uncomp_size;
  }

protected:
  int underflow() override {
    if (m_remaining <= 0) {
      return traits_type::eof();
    }

    const std::uint64_t amount =
        std::min<std::uint64_t>(m_remaining, m_buffer.size());
    const std::uint32_t result =
        mz_zip_reader_extract_iter_read(m_iter, m_buffer.data(), amount);
    // miniz reports a failed inflate as a short read; leaving the get area
    // empty while claiming success would spin `underflow` forever.
    if (result == 0) {
      return traits_type::eof();
    }
    m_remaining -= result;
    setg(m_buffer.data(), m_buffer.data(), m_buffer.data() + result);

    return traits_type::to_int_type(*gptr());
  }

private:
  std::shared_ptr<const Archive> m_archive;
  mz_zip_reader_extract_iter_state *m_iter{};
  std::uint64_t m_remaining{0};
  std::vector<char> m_buffer;
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
  FileInZip(std::shared_ptr<const Archive> archive, const std::uint32_t index)
      : m_archive{std::move(archive)}, m_index{index} {
    if (m_archive == nullptr) {
      throw NullPointerError("FileInZip: archive is nullptr");
    }
  }

  [[nodiscard]] FileLocation location() const noexcept override {
    return m_archive->file()->location();
  }
  [[nodiscard]] std::size_t size() const override {
    mz_zip_archive_file_stat stat{};
    mz_zip_reader_file_stat(m_archive->zip(), m_index, &stat);
    return stat.m_uncomp_size;
  }

  [[nodiscard]] std::optional<AbsPath> disk_path() const override {
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string_view> memory_data() const override {
    return std::nullopt;
  }

  [[nodiscard]] std::unique_ptr<std::istream> stream() const override {
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
  return !mz_zip_reader_is_file_a_directory(m_archive->zip(), m_index);
}

bool Archive::Entry::is_directory() const {
  return mz_zip_reader_is_file_a_directory(m_archive->zip(), m_index);
}

RelPath Archive::Entry::path() const {
  std::array<char, MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE> filename{};
  mz_zip_reader_get_filename(m_archive->zip(), m_index, filename.data(),
                             static_cast<mz_uint>(filename.size()));
  return RelPath(filename.data());
}

Method Archive::Entry::method() const {
  mz_zip_archive_file_stat stat{};
  mz_zip_reader_file_stat(m_archive->zip(), m_index, &stat);
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
  return std::make_shared<FileInZip>(m_archive->shared_from_this(), m_index);
}

ReadSource::ReadSource(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  if (m_file == nullptr) {
    throw NullPointerError("ReadSource: file is nullptr");
  }
  m_memory = m_file->memory_data();
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

  std::unique_ptr<std::istream> stream;
  {
    std::lock_guard lock(m_mutex);
    if (!m_streams.empty()) {
      stream = std::move(m_streams.back());
      m_streams.pop_back();
    }
  }
  if (stream == nullptr) {
    stream = m_file->stream();
  }

  // A short read has to surface as one. Clear first so an earlier read past the
  // end does not poison every later seek.
  stream->clear();
  stream->seekg(static_cast<std::streamoff>(offset));
  stream->read(static_cast<char *>(buffer), static_cast<std::streamsize>(size));
  const auto result = static_cast<std::size_t>(stream->gcount());

  {
    std::lock_guard lock(m_mutex);
    m_streams.push_back(std::move(stream));
  }

  return result;
}

Archive::Archive(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  if (m_file == nullptr) {
    throw NullPointerError("Archive: file is nullptr");
  }
  m_source = std::make_unique<ReadSource>(m_file);
  open_from_file(m_zip, *m_file, *m_source);
}

Archive::~Archive() { mz_zip_end(&m_zip); }

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
    in->read(static_cast<char *>(buffer), static_cast<std::streamsize>(s));
    return in->gcount();
  };

  // Without the flag the size only lands in a trailing data descriptor, which
  // LibreOffice rejects on a stored entry.
  return mz_zip_writer_add_read_buf_callback(
      &archive, path.c_str(), read_callback, &istream, size, &time,
      comment.c_str(), comment.size(),
      level_and_flags | MZ_ZIP_FLAG_WRITE_HEADER_SET_SIZE, "", 0, "", 0);
}

} // namespace odr::internal::zip
