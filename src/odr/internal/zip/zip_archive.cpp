#include <odr/internal/zip/zip_archive.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/zip/zip_exceptions.hpp>
#include <odr/internal/zip/zip_util.hpp>

#include <string>
#include <utility>

#include <miniz/miniz.h>

namespace odr::internal::zip {

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

ZipArchive::ZipArchive(const std::shared_ptr<util::Archive> &archive) {
  for (auto &&entry : *archive) {
    if (entry.is_file()) {
      std::uint8_t compression_level = 6;
      if (entry.method() == util::Method::STORED) {
        compression_level = 0;
      }
      insert_file(end(), entry.path(), entry.file(), compression_level);
    } else if (entry.is_directory()) {
      insert_directory(end(), entry.path());
    }
  }
}

std::shared_ptr<abstract::Filesystem> ZipArchive::as_filesystem() const {
  // TODO return an actual filesystem view
  auto filesystem = std::make_shared<common::VirtualFilesystem>();

  for (const auto &e : *this) {
    common::Path path = e.path();
    if (path.relative()) {
      path = common::Path("/").join(path);
    }

    if (e.is_directory()) {
      filesystem->create_directory(path);
    } else if (e.is_file()) {
      filesystem->copy(e.file(), path);
    }
  }

  return filesystem;
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
    throw MinizSaveError(archive);
  }

  for (auto &&entry : *this) {
    auto path = entry.path();
    if (path.absolute()) {
      path = path.rebase(common::Path("/"));
    }

    if (entry.is_file()) {
      auto file = entry.file();
      auto istream = file->stream();
      auto size = file->size();

      state = util::append_file(archive, path.string(), *istream, size, time,
                                "", entry.compression_level());
      if (!state) {
        throw MinizSaveError(archive);
      }
    } else if (entry.is_directory()) {
      state = mz_zip_writer_add_mem(&archive, (path.string() + "/").c_str(),
                                    nullptr, 0, 0);
      if (!state) {
        throw MinizSaveError(archive);
      }
    } else {
      throw ZipSaveError();
    }
  }

  state = mz_zip_writer_finalize_archive(&archive);
  if (!state) {
    throw MinizSaveError(archive);
  }
  state = mz_zip_writer_end(&archive);
  if (!state) {
    throw MinizSaveError(archive);
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
  return m_entries.insert(at, ZipArchive::Entry(std::move(path), nullptr, 0));
}

} // namespace odr::internal::zip
