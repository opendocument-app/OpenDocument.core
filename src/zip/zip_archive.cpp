#include <abstract/file.h>
#include <chrono>
#include <common/path.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <zip/miniz_util.h>
#include <zip/zip_archive.h>

namespace odr::zip {

namespace {
class FileInZipIstream final : public std::istream {
public:
  FileInZipIstream(std::shared_ptr<ReadonlyZipArchive> persist,
                   std::unique_ptr<miniz::ReaderBuffer> sbuf)
      : std::istream(sbuf.get()), m_persist{std::move(persist)},
        m_sbuf{std::move(sbuf)} {}
  FileInZipIstream(std::shared_ptr<ReadonlyZipArchive> persist,
                   mz_zip_reader_extract_iter_state *iter)
      : FileInZipIstream(std::move(persist),
                         std::make_unique<miniz::ReaderBuffer>(iter)) {}

private:
  std::shared_ptr<ReadonlyZipArchive> m_persist;
  std::unique_ptr<miniz::ReaderBuffer> m_sbuf;
};

class FileInZip final : public abstract::File {
public:
  FileInZip(std::shared_ptr<ReadonlyZipArchive> persist,
            const FileLocation location, mz_zip_archive &zip,
            const std::uint32_t index)
      : m_persist{std::move(persist)},
        m_location{location}, m_zip{zip}, m_index{index} {}

  [[nodiscard]] FileLocation location() const noexcept final {
    return m_location;
  }

  [[nodiscard]] std::size_t size() const final {
    mz_zip_archive_file_stat stat{};
    mz_zip_reader_file_stat(&m_zip, m_index, &stat);
    return stat.m_uncomp_size;
  }

  [[nodiscard]] std::unique_ptr<std::istream> read() const final {
    auto iter = mz_zip_reader_extract_iter_new(&m_zip, m_index, 0);
    return std::make_unique<FileInZipIstream>(m_persist, iter);
  }

private:
  std::shared_ptr<ReadonlyZipArchive> m_persist;
  FileLocation m_location;
  mz_zip_archive &m_zip;
  std::uint32_t m_index;
};
} // namespace

ReadonlyZipArchive::Entry::Entry(const ReadonlyZipArchive &parent,
                                 std::uint32_t index)
    : m_parent{parent}, m_index{index} {}

bool ReadonlyZipArchive::Entry::is_file() const {
  return !mz_zip_reader_is_file_a_directory(&m_parent.m_zip, m_index);
}

bool ReadonlyZipArchive::Entry::is_directory() const {
  return mz_zip_reader_is_file_a_directory(&m_parent.m_zip, m_index);
}

common::Path ReadonlyZipArchive::Entry::path() const {
  char filename[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];
  mz_zip_reader_get_filename(&m_parent.m_zip, m_index, filename,
                             MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE);
  return common::Path(filename);
}

Method ReadonlyZipArchive::Entry::method() const {
  mz_zip_archive_file_stat stat{};
  mz_zip_reader_file_stat(&m_parent.m_zip, m_index, &stat);
  switch (stat.m_method) {
  case 0:
    return Method::STORED;
  case MZ_DEFLATED:
    return Method::DEFLATED;
  }
  return Method::UNSUPPORTED;
}

std::unique_ptr<abstract::File> ReadonlyZipArchive::Entry::file() const {
  return file({});
}

std::unique_ptr<abstract::File> ReadonlyZipArchive::Entry::file(
    std::shared_ptr<ReadonlyZipArchive> persist) const {
  if (!is_file()) {
    return {};
  }
  return std::make_unique<FileInZip>(
      std::move(persist), m_parent.m_file->location(), m_parent.m_zip, m_index);
}

ReadonlyZipArchive::Iterator::Iterator(const ReadonlyZipArchive &zip,
                                       const std::uint32_t index)
    : m_entry{zip, index} {}

ReadonlyZipArchive::Iterator::reference
ReadonlyZipArchive::Iterator::operator*() const {
  return m_entry;
}

ReadonlyZipArchive::Iterator::pointer
ReadonlyZipArchive::Iterator::operator->() const {
  return &m_entry;
}

bool ReadonlyZipArchive::Iterator::operator==(const Iterator &other) const {
  return m_entry.m_index == other.m_entry.m_index;
};

bool ReadonlyZipArchive::Iterator::operator!=(const Iterator &other) const {
  return m_entry.m_index != other.m_entry.m_index;
};

ReadonlyZipArchive::Iterator &ReadonlyZipArchive::Iterator::operator++() {
  m_entry.m_index++;
  return *this;
}

ReadonlyZipArchive::Iterator ReadonlyZipArchive::Iterator::operator++(int) {
  Iterator tmp = *this;
  ++(*this);
  return tmp;
}

ReadonlyZipArchive::ReadonlyZipArchive(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)}, m_data{m_file->read()} {
  m_zip.m_pIO_opaque = m_data.get();
  m_zip.m_pRead = [](void *opaque, std::uint64_t offset, void *buffer,
                     std::size_t size) {
    auto in = static_cast<std::istream *>(opaque);
    in->seekg(offset);
    in->read(static_cast<char *>(buffer), size);
    return size;
  };
  const bool state = mz_zip_reader_init(
      &m_zip, file->size(), MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY);
  if (!state) {
    throw NoZipFile();
  }
}

ReadonlyZipArchive::Iterator ReadonlyZipArchive::begin() const {
  return Iterator(*this, 0);
}

ReadonlyZipArchive::Iterator ReadonlyZipArchive::end() const {
  return Iterator(*this, mz_zip_reader_get_num_files(&m_zip));
}

ReadonlyZipArchive::Iterator
ReadonlyZipArchive::find(const common::Path &path) const {
  for (auto it = begin(); it != end(); ++it) {
    if (it->path() == path) {
      return it;
    }
  }

  return end();
}

ZipArchive::Entry::Entry(common::Path path,
                         std::shared_ptr<abstract::File> file,
                         std::uint32_t compression_level)
    : m_path{std::move(path)}, m_file{std::move(file)},
      m_compression_level{compression_level} {}

bool ZipArchive::Entry::is_file() const { return m_file.operator bool(); }

bool ZipArchive::Entry::is_directory() const { return !m_file; }

common::Path ZipArchive::Entry::path() const { return m_path; }

std::shared_ptr<abstract::File> ZipArchive::Entry::file() const {
  return m_file;
}

void ZipArchive::Entry::file(std::shared_ptr<abstract::File> file) {
  m_file = std::move(file);
}

ZipArchive::ZipArchive() = default;

ZipArchive::ZipArchive(std::shared_ptr<abstract::File> file)
    : ZipArchive(ReadonlyZipArchive(std::move(file))) {}

ZipArchive::ZipArchive(ReadonlyZipArchive archive)
    : ZipArchive(std::make_shared<ReadonlyZipArchive>(std::move(archive))) {}

ZipArchive::ZipArchive(const std::shared_ptr<ReadonlyZipArchive> &archive) {
  for (auto &&entry : *archive) {
    if (entry.is_file()) {
      std::uint8_t compression_level = 6;
      if (entry.method() == Method::STORED) {
        compression_level = 0;
      }
      insert_file(end(), entry.path(), entry.file(archive), compression_level);
    } else if (entry.is_directory()) {
      insert_directory(end(), entry.path());
    }
  }
}

ZipArchive::Iterator ZipArchive::begin() const {
  return std::cbegin(m_entries);
}

ZipArchive::Iterator ZipArchive::end() const { return std::cend(m_entries); }

ZipArchive::Iterator ZipArchive::find(const common::Path &path) const {
  for (auto it = begin(); it != end(); ++it) {
    if (it->path() == path) {
      return it;
    }
  }

  return end();
}

ZipArchive::Iterator
ZipArchive::insert_file(Iterator at, common::Path path,
                        std::shared_ptr<abstract::File> file,
                        std::uint32_t compression_level) {
  m_entries.insert(at, ZipArchive::Entry(std::move(path), std::move(file),
                                         compression_level));
}

ZipArchive::Iterator ZipArchive::insert_directory(Iterator at,
                                                  common::Path path) {
  m_entries.insert(at, ZipArchive::Entry(std::move(path), {}, 0));
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

  for (auto &&entry : *this) {
    auto path = entry.path();

    if (entry.is_file()) {
      auto file = entry.file();
      auto istream = file->read();
      auto size = file->size();

      // TODO use level of entry
      state = miniz::append_file(archive, path.string(), *istream, size, time,
                                 "", 6);
      if (!state) {
        throw ZipSaveError();
      }
    } else if (entry.is_directory()) {
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

} // namespace odr::zip
