#pragma once

#include <odr/internal/abstract/archive.hpp>
#include <odr/internal/common/path.hpp>

#include <cstdint>
#include <iosfwd>
#include <iterator>
#include <memory>
#include <vector>

namespace odr::internal::abstract {
class File;
}

namespace odr::internal::common {
class MemoryFile;
class DiskFile;
} // namespace odr::internal::common

namespace odr::internal::zip {
namespace util {
class Archive;
}

class ZipArchive final : public abstract::Archive {
public:
  ZipArchive();
  explicit ZipArchive(const std::shared_ptr<util::Archive> &archive);

  [[nodiscard]] std::shared_ptr<abstract::Filesystem>
  as_filesystem() const final;

  void save(std::ostream &out) const final;

  class Entry;

  using Iterator = std::vector<Entry>::const_iterator;

  [[nodiscard]] Iterator begin() const;
  [[nodiscard]] Iterator end() const;

  [[nodiscard]] Iterator find(const Path &path) const;

  Iterator insert_file(Iterator at, Path path,
                       std::shared_ptr<abstract::File> file,
                       std::uint32_t compression_level = 6);
  Iterator insert_directory(Iterator at, Path path);

  class Entry {
  public:
    Entry(Path path, std::shared_ptr<abstract::File> file,
          std::uint32_t compression_level);

    [[nodiscard]] bool is_file() const;
    [[nodiscard]] bool is_directory() const;
    [[nodiscard]] Path path() const;
    [[nodiscard]] std::shared_ptr<abstract::File> file() const;
    [[nodiscard]] std::uint32_t compression_level() const;

    void file(std::shared_ptr<abstract::File> file);

  private:
    Path m_path;
    std::shared_ptr<abstract::File> m_file;
    std::uint32_t m_compression_level{6};

    friend Iterator;
  };

private:
  std::vector<Entry> m_entries;
};

} // namespace odr::internal::zip
