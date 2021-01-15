#include <abstract/file.h>
#include <common/path.h>
#include <odr/file.h>
#include <streambuf>
#include <zip/zip_archive.h>
#include <zip/zip_file.h>

namespace odr::zip {

namespace {
constexpr std::uint64_t buffer_size = 4098;

class FileInZipStreambuf final : public std::streambuf {
public:
  FileInZipStreambuf(std::shared_ptr<const ZipFile> file,
                     mz_zip_reader_extract_iter_state *iter)
      : m_file{std::move(file)}, m_iter{iter},
        m_remaining{iter->file_stat.m_uncomp_size},
        m_buffer{new char[buffer_size]} {}

  ~FileInZipStreambuf() final {
    mz_zip_reader_extract_iter_free(m_iter);
    delete[] m_buffer;
  }

  int underflow() final {
    if (m_remaining <= 0)
      return std::char_traits<char>::eof();

    const std::uint64_t amount = std::min(m_remaining, buffer_size);
    const std::uint32_t result =
        mz_zip_reader_extract_iter_read(m_iter, m_buffer, amount);
    m_remaining -= result;
    this->setg(this->m_buffer, this->m_buffer, this->m_buffer + result);

    return std::char_traits<char>::to_int_type(*gptr());
  }

private:
  std::shared_ptr<const ZipFile> m_file;

  mz_zip_reader_extract_iter_state *m_iter;
  std::uint64_t m_remaining;
  char *m_buffer;
};

class FileInZipIstream final : public std::istream {
public:
  explicit FileInZipIstream(std::unique_ptr<FileInZipStreambuf> sbuf)
      : std::istream(sbuf.get()), m_sbuf{std::move(sbuf)} {}
  FileInZipIstream(std::shared_ptr<const ZipFile> file,
                   mz_zip_reader_extract_iter_state *iter)
      : FileInZipIstream(
            std::make_unique<FileInZipStreambuf>(std::move(file), iter)) {}

private:
  std::unique_ptr<FileInZipStreambuf> m_sbuf;
};

class FileInZip final : public abstract::File {
public:
  FileInZip(std::shared_ptr<const ZipFile> file, const std::uint32_t index,
            const std::size_t size)
      : m_file{std::move(file)}, m_index{index}, m_size{size} {}

  [[nodiscard]] FileType file_type() const noexcept final {
    return FileType::UNKNOWN;
  }

  [[nodiscard]] FileCategory file_category() const noexcept final {
    return FileCategory::UNKNOWN;
  }

  [[nodiscard]] FileMeta file_meta() const noexcept final {
    return {}; // TODO
  }

  [[nodiscard]] FileLocation file_location() const noexcept final {
    return m_file->file_location();
  }

  [[nodiscard]] std::size_t size() const final { return m_size; }

  [[nodiscard]] std::unique_ptr<std::istream> data() const final {
    auto iter = mz_zip_reader_extract_iter_new(m_file->impl(), m_index, 0);
    return std::make_unique<FileInZipIstream>(m_file, iter);
  }

private:
  std::shared_ptr<const ZipFile> m_file;
  std::uint32_t m_index;
  std::size_t m_size;
};
} // namespace

ZipArchive::ZipArchive() = default;

ZipArchive::ZipArchive(std::shared_ptr<const ZipFile> file)
    : m_file{std::move(file)} {
  auto num_files = mz_zip_reader_get_num_files(m_file->impl());
  for (std::uint32_t i = 0; i < num_files; ++i) {
    mz_zip_archive_file_stat stat{};
    mz_zip_reader_file_stat(m_file->impl(), i, &stat);

    if (stat.m_is_directory) {
      DefaultArchive::insert_directory(DefaultArchive::end(), stat.m_filename);
    } else {
      std::uint8_t compression_level = 6;
      if (stat.m_method == 0) {
        compression_level = 0;
      }
      ZipArchive::insert_file(
          DefaultArchive::end(), stat.m_filename,
          std::make_shared<FileInZip>(m_file, i, stat.m_uncomp_size),
          compression_level);
    }
  }
}

std::unique_ptr<abstract::ArchiveEntryIterator>
ZipArchive::insert_file(std::unique_ptr<abstract::ArchiveEntryIterator> at,
                        common::Path path,
                        std::shared_ptr<abstract::File> file) {
  return insert(std::move(at), std::make_unique<ZipArchiveEntry>(
                                   std::move(path), std::move(file)));
}

std::unique_ptr<abstract::ArchiveEntryIterator>
ZipArchive::insert_file(std::unique_ptr<abstract::ArchiveEntryIterator> at,
                        common::Path path, std::shared_ptr<abstract::File> file,
                        const std::uint8_t compression_level) {
  return insert(std::move(at),
                std::make_unique<ZipArchiveEntry>(
                    std::move(path), std::move(file), compression_level));
}

void ZipArchive::save(const common::Path &path) const {
  // TODO
}

ZipArchiveEntry::ZipArchiveEntry(common::Path path,
                                 std::shared_ptr<abstract::File> file)
    : DefaultArchiveEntry(std::move(path), std::move(file)) {}

ZipArchiveEntry::ZipArchiveEntry(common::Path path,
                                 std::shared_ptr<abstract::File> file,
                                 const std::uint8_t compression_level)
    : DefaultArchiveEntry(std::move(path), std::move(file)),
      m_compression_level{compression_level} {}

std::uint8_t ZipArchiveEntry::compression_level() const {
  return m_compression_level;
}

void ZipArchiveEntry::compression_level(const std::uint8_t compression_level) {
  m_compression_level = compression_level;
}

} // namespace odr::zip
