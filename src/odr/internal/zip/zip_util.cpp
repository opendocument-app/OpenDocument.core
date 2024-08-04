#include <odr/internal/zip/zip_util.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/common/file.hpp>

#include <algorithm>
#include <utility>

namespace odr::internal::zip::util {

namespace {

class ReaderBuffer final : public std::streambuf {
public:
  explicit ReaderBuffer(mz_zip_reader_extract_iter_state *iter)
      : ReaderBuffer(iter, 4098) {}
  ReaderBuffer(mz_zip_reader_extract_iter_state *iter,
               std::size_t buffer_size) {
    if (iter == nullptr) {
      throw std::invalid_argument("ReaderBuffer: iter is nullptr");
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

    const std::uint64_t amount = std::min(m_remaining, m_buffer_size);
    const std::uint32_t result =
        mz_zip_reader_extract_iter_read(m_iter, m_buffer, amount);
    m_remaining -= result;
    setg(m_buffer, m_buffer, m_buffer + result);

    return std::char_traits<char>::to_int_type(*gptr());
  }

private:
  mz_zip_reader_extract_iter_state *m_iter{};
  std::uint64_t m_remaining{0};
  std::uint64_t m_buffer_size{4098};
  char *m_buffer;
};

class FileInZipIstream final : public std::istream {
public:
  FileInZipIstream(std::shared_ptr<Archive> archive,
                   std::unique_ptr<ReaderBuffer> sbuf)
      : std::istream(sbuf.get()), m_archive{std::move(archive)},
        m_sbuf{std::move(sbuf)} {
    if (m_archive == nullptr) {
      throw std::invalid_argument("FileInZipIstream: archive is nullptr");
    }
    if (m_sbuf == nullptr) {
      throw std::invalid_argument("FileInZipIstream: sbuf is nullptr");
    }
  }
  FileInZipIstream(std::shared_ptr<Archive> archive,
                   mz_zip_reader_extract_iter_state *iter)
      : FileInZipIstream(std::move(archive),
                         std::make_unique<ReaderBuffer>(iter)) {}

private:
  std::shared_ptr<Archive> m_archive;
  std::unique_ptr<ReaderBuffer> m_sbuf;
};

class FileInZip final : public abstract::File {
public:
  FileInZip(std::shared_ptr<Archive> archive, std::uint32_t index)
      : m_archive{std::move(archive)}, m_index{index} {
    if (m_archive == nullptr) {
      throw std::invalid_argument("FileInZip: archive is nullptr");
    }
  }

  [[nodiscard]] FileLocation location() const noexcept final {
    return m_archive->file()->location();
  }
  [[nodiscard]] std::size_t size() const final {
    mz_zip_archive_file_stat stat{};
    mz_zip_reader_file_stat(const_cast<mz_zip_archive *>(m_archive->zip()),
                            m_index, &stat);
    return stat.m_uncomp_size;
  }

  [[nodiscard]] std::optional<common::Path> disk_path() const final {
    return std::nullopt;
  }
  [[nodiscard]] const char *memory_data() const final { return nullptr; }

  [[nodiscard]] std::unique_ptr<std::istream> stream() const final {
    if (mz_zip_reader_is_file_encrypted(
            const_cast<mz_zip_archive *>(m_archive->zip()), m_index)) {
      return nullptr;
    }
    if (!mz_zip_reader_is_file_supported(
            const_cast<mz_zip_archive *>(m_archive->zip()), m_index)) {
      return nullptr;
    }
    auto iter = mz_zip_reader_extract_iter_new(
        const_cast<mz_zip_archive *>(m_archive->zip()), m_index, 0);
    if (iter == nullptr) {
      return nullptr;
    }
    return std::make_unique<FileInZipIstream>(m_archive, iter);
  }

private:
  std::shared_ptr<Archive> m_archive;
  std::uint32_t m_index;
};

} // namespace

Archive::Entry::Entry(const Archive &parent, std::uint32_t index)
    : m_parent{parent}, m_index{index} {}

bool Archive::Entry::is_file() const {
  return !mz_zip_reader_is_file_a_directory(
      const_cast<mz_zip_archive *>(m_parent.zip()), m_index);
}

bool Archive::Entry::is_directory() const {
  return mz_zip_reader_is_file_a_directory(
      const_cast<mz_zip_archive *>(m_parent.zip()), m_index);
}

common::Path Archive::Entry::path() const {
  char filename[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];
  mz_zip_reader_get_filename(const_cast<mz_zip_archive *>(m_parent.zip()),
                             m_index, filename,
                             MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE);
  return {filename};
}

Method Archive::Entry::method() const {
  mz_zip_archive_file_stat stat{};
  mz_zip_reader_file_stat(const_cast<mz_zip_archive *>(m_parent.zip()), m_index,
                          &stat);
  switch (stat.m_method) {
  case 0:
    return Method::STORED;
  case MZ_DEFLATED:
    return Method::DEFLATED;
  }
  return Method::UNSUPPORTED;
}

std::shared_ptr<abstract::File>
Archive::Entry::file(std::shared_ptr<Archive> archive) const {
  if (!is_file()) {
    return nullptr;
  }
  return std::make_shared<FileInZip>(std::move(archive), m_index);
}

Archive::Iterator::Iterator(const Archive &zip, const std::uint32_t index)
    : m_entry{zip, index} {}

Archive::Iterator::reference Archive::Iterator::operator*() const {
  return m_entry;
}

Archive::Iterator::pointer Archive::Iterator::operator->() const {
  return &m_entry;
}

bool Archive::Iterator::operator==(const Iterator &other) const {
  return m_entry.m_index == other.m_entry.m_index;
}

bool Archive::Iterator::operator!=(const Iterator &other) const {
  return m_entry.m_index != other.m_entry.m_index;
}

Archive::Iterator &Archive::Iterator::operator++() {
  m_entry.m_index++;
  return *this;
}

Archive::Iterator Archive::Iterator::operator++(int) {
  Iterator tmp = *this;
  ++(*this);
  return tmp;
}

Archive::Archive(const std::shared_ptr<common::MemoryFile> &file)
    : Archive(std::dynamic_pointer_cast<abstract::File>(file)) {}

Archive::Archive(const std::shared_ptr<common::DiskFile> &file)
    : Archive(std::dynamic_pointer_cast<abstract::File>(file)) {}

Archive::Archive(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  if (m_file == nullptr) {
    throw std::invalid_argument("Archive: file is nullptr");
  }
  m_stream = m_file->stream();
  open_from_file(m_zip, *m_file, *m_stream);
}

Archive::Archive(const Archive &other) : Archive(other.m_file) {}

Archive::Archive(Archive &&other) noexcept = default;

Archive::~Archive() { mz_zip_end(&m_zip); }

Archive &Archive::operator=(const Archive &other) {
  if (&other != this) {
    m_zip = other.m_zip;
    m_file = other.m_file;
  }
  return *this;
}

Archive &Archive::operator=(Archive &&) noexcept = default;

const mz_zip_archive *Archive::zip() const { return &m_zip; }

std::shared_ptr<abstract::File> Archive::file() const noexcept {
  return m_file;
}

Archive::Iterator Archive::begin() const {
  return {const_cast<Archive &>(*this), 0};
}

Archive::Iterator Archive::end() const {
  return {const_cast<Archive &>(*this),
          mz_zip_reader_get_num_files(const_cast<mz_zip_archive *>(&m_zip))};
}

Archive::Iterator Archive::find(const common::Path &path) const {
  for (auto it = begin(); it != end(); ++it) {
    if (it->path() == path) {
      return it;
    }
  }

  return end();
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
