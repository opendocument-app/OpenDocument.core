#include <chrono>
#include <internal/abstract/file.h>
#include <internal/common/path.h>
#include <internal/zip/zip_archive.h>
#include <internal/zip/zip_util.h>
#include <miniz/miniz.h>
#include <miniz/miniz_zip.h>
#include <odr/exceptions.h>
#include <string>
#include <utility>
#include <vector>

namespace odr::internal::zip {

ReadonlyZipArchive::Entry::Entry(const ReadonlyZipArchive &parent,
                                 std::uint32_t index)
    : m_parent{parent}, m_index{index} {}

bool ReadonlyZipArchive::Entry::is_file() const {
  return !mz_zip_reader_is_file_a_directory(m_parent.m_zip->zip(), m_index);
}

bool ReadonlyZipArchive::Entry::is_directory() const {
  return mz_zip_reader_is_file_a_directory(m_parent.m_zip->zip(), m_index);
}

common::Path ReadonlyZipArchive::Entry::path() const {
  char filename[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];
  mz_zip_reader_get_filename(m_parent.m_zip->zip(), m_index, filename,
                             MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE);
  return {filename};
}

Method ReadonlyZipArchive::Entry::method() const {
  mz_zip_archive_file_stat stat{};
  mz_zip_reader_file_stat(m_parent.m_zip->zip(), m_index, &stat);
  switch (stat.m_method) {
  case 0:
    return Method::STORED;
  case MZ_DEFLATED:
    return Method::DEFLATED;
  }
  return Method::UNSUPPORTED;
}

std::unique_ptr<abstract::File> ReadonlyZipArchive::Entry::file() const {
  if (!is_file()) {
    return {};
  }
  return std::make_unique<util::FileInZip>(m_parent.m_zip, m_index);
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

ReadonlyZipArchive::ReadonlyZipArchive(
    const std::shared_ptr<common::MemoryFile> &file)
    : m_zip{std::make_shared<util::Archive>(file)} {}

ReadonlyZipArchive::ReadonlyZipArchive(
    const std::shared_ptr<common::DiskFile> &file)
    : m_zip{std::make_shared<util::Archive>(file)} {}

ReadonlyZipArchive::Iterator ReadonlyZipArchive::begin() const {
  return {*this, 0};
}

ReadonlyZipArchive::Iterator ReadonlyZipArchive::end() const {
  return {*this, mz_zip_reader_get_num_files(m_zip->zip())};
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

std::uint32_t ZipArchive::Entry::compression_level() const {
  return m_compression_level;
}

void ZipArchive::Entry::file(std::shared_ptr<abstract::File> file) {
  m_file = std::move(file);
}

ZipArchive::ZipArchive() = default;

ZipArchive::ZipArchive(const std::shared_ptr<common::MemoryFile> &file)
    : ZipArchive(ReadonlyZipArchive(file)) {}

ZipArchive::ZipArchive(const std::shared_ptr<common::DiskFile> &file)
    : ZipArchive(ReadonlyZipArchive(file)) {}

ZipArchive::ZipArchive(ReadonlyZipArchive archive)
    : ZipArchive(std::make_shared<ReadonlyZipArchive>(std::move(archive))) {}

ZipArchive::ZipArchive(const std::shared_ptr<ReadonlyZipArchive> &archive) {
  for (auto &&entry : *archive) {
    if (entry.is_file()) {
      std::uint8_t compression_level = 6;
      if (entry.method() == Method::STORED) {
        compression_level = 0;
      }
      insert_file(end(), entry.path(), entry.file(), compression_level);
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
  return m_entries.insert(
      at,
      ZipArchive::Entry(std::move(path), std::move(file), compression_level));
}

ZipArchive::Iterator ZipArchive::insert_directory(Iterator at,
                                                  common::Path path) {
  return m_entries.insert(at, ZipArchive::Entry(std::move(path), {}, 0));
}

bool ZipArchive::move(common::Path, common::Path) { return false; }

bool ZipArchive::remove(common::Path) { return false; }

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
      auto istream = file->stream();
      auto size = file->size();

      state = util::append_file(archive, path.string(), *istream, size, time,
                                "", entry.compression_level());
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

} // namespace odr::internal::zip
