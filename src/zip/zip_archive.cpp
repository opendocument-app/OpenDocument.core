#include <abstract/file.h>
#include <chrono>
#include <common/path.h>
#include <odr/archive.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <zip/miniz_util.h>
#include <zip/zip_archive.h>
#include <zip/zip_file.h>

namespace odr::zip {

namespace {
class FileInZipStreambuf final : public miniz::ReaderBuffer {
public:
  FileInZipStreambuf(std::shared_ptr<const ZipFile> file,
                     mz_zip_reader_extract_iter_state *iter)
      : ReaderBuffer(iter), m_file(std::move(file)) {}

private:
  std::shared_ptr<const ZipFile> m_file;
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

ZipArchive::ZipArchive(const std::shared_ptr<const ZipFile> &file) {
  auto num_files = mz_zip_reader_get_num_files(file->impl());
  for (std::uint32_t i = 0; i < num_files; ++i) {
    mz_zip_archive_file_stat stat{};
    mz_zip_reader_file_stat(file->impl(), i, &stat);

    if (stat.m_is_directory) {
      DefaultArchive::insert_directory(DefaultArchive::end(), stat.m_filename);
    } else {
      std::uint8_t compression_level = 6;
      if (stat.m_method == 0) {
        compression_level = 0;
      }
      ZipArchive::insert_file(
          DefaultArchive::end(), stat.m_filename,
          std::make_shared<FileInZip>(file, i, stat.m_uncomp_size),
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

void ZipArchive::save(std::ostream &out) const {
  bool state;
  const auto time =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  mz_zip_archive archive{};
  archive.m_pIO_opaque = &out;
  archive.m_pWrite = [](void *opaque, std::uint64_t /*offset*/,
                        const void *buffer, std::size_t size) {
    auto out = static_cast<std::ostream *>(opaque);
    out->write(static_cast<const char *>(buffer), size);
    return size;
  };
  state = mz_zip_writer_init(&archive, 0);
  if (!state) {
    throw ZipSaveError();
  }

  for (auto it = begin(); !it->equals(*end()); it->next()) {
    auto entry = it->entry();
    auto type = entry->type();
    auto path = entry->path();

    if (type == ArchiveEntryType::FILE) {
      auto file = entry->file();
      auto istream = file->data();
      auto size = file->size();

      // TODO use level of entry
      state = miniz::append_file(archive, path.string(), *istream, size, time,
                                 "", 6);
      if (!state) {
        throw ZipSaveError();
      }
    } else if (type == ArchiveEntryType::DIRECTORY) {
      state = mz_zip_writer_add_mem(&archive, (path.string() + "/").c_str(),
                                    nullptr, 0, 0);
      if (!state) {
        throw ZipSaveError();
      }
    } else {
      throw ZipSaveError();
    }
  }

  state = mz_zip_writer_finalize_archive(&archive);
  if (!state) {
    throw ZipSaveError();
  }
  state = mz_zip_writer_end(&archive);
  if (!state) {
    throw ZipSaveError();
  }
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
