#ifndef ODR_INTERNAL_ZIP_ARCHIVE_H
#define ODR_INTERNAL_ZIP_ARCHIVE_H

#include <cstdint>
#include <iosfwd>
#include <iterator>
#include <memory>
#include <miniz/miniz.h>
#include <odr/internal/common/path.h>
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

enum class Method {
  UNSUPPORTED,
  STORED,
  DEFLATED,
};

class ReadonlyZipArchive final {
public:
  explicit ReadonlyZipArchive(const std::shared_ptr<common::MemoryFile> &file);
  explicit ReadonlyZipArchive(const std::shared_ptr<common::DiskFile> &file);

  class Iterator;

  [[nodiscard]] Iterator begin() const;
  [[nodiscard]] Iterator end() const;

  [[nodiscard]] Iterator find(const common::Path &path) const;

  class Entry {
  public:
    Entry(const ReadonlyZipArchive &parent, std::uint32_t index);

    [[nodiscard]] bool is_file() const;
    [[nodiscard]] bool is_directory() const;
    [[nodiscard]] common::Path path() const;
    [[nodiscard]] Method method() const;
    [[nodiscard]] std::unique_ptr<abstract::File> file() const;

  private:
    const ReadonlyZipArchive &m_parent;
    std::uint32_t m_index;

    friend Iterator;
  };

  class Iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = Entry;
    using pointer = const Entry *;
    using reference = const Entry &;

    Iterator(const ReadonlyZipArchive &zip, std::uint32_t index);

    reference operator*() const;
    pointer operator->() const;

    bool operator==(const Iterator &other) const;
    bool operator!=(const Iterator &other) const;

    Iterator &operator++();
    Iterator operator++(int);

  private:
    Entry m_entry;
  };

private:
  std::shared_ptr<util::Archive> m_zip;
};

class ZipArchive final {
public:
  ZipArchive();
  explicit ZipArchive(const std::shared_ptr<common::MemoryFile> &file);
  explicit ZipArchive(const std::shared_ptr<common::DiskFile> &file);
  explicit ZipArchive(ReadonlyZipArchive archive);
  explicit ZipArchive(const std::shared_ptr<ReadonlyZipArchive> &archive);

  class Entry;

  using Iterator = std::vector<Entry>::const_iterator;

  [[nodiscard]] Iterator begin() const;
  [[nodiscard]] Iterator end() const;

  [[nodiscard]] Iterator find(const common::Path &path) const;

  Iterator insert_file(Iterator at, common::Path path,
                       std::shared_ptr<abstract::File> file,
                       std::uint32_t compression_level = 6);
  Iterator insert_directory(Iterator at, common::Path path);

  bool move(common::Path from, common::Path to);

  bool remove(common::Path path);

  void save(std::ostream &out) const;

  class Entry {
  public:
    Entry(common::Path path, std::shared_ptr<abstract::File> file,
          std::uint32_t compression_level);

    [[nodiscard]] bool is_file() const;
    [[nodiscard]] bool is_directory() const;
    [[nodiscard]] common::Path path() const;
    [[nodiscard]] std::shared_ptr<abstract::File> file() const;
    [[nodiscard]] std::uint32_t compression_level() const;

    void file(std::shared_ptr<abstract::File> file);

  private:
    common::Path m_path;
    std::shared_ptr<abstract::File> m_file;
    std::uint32_t m_compression_level{6};

    friend Iterator;
  };

private:
  std::vector<Entry> m_entries;
};

} // namespace odr::internal::zip

#endif // ODR_INTERNAL_ZIP_ARCHIVE_H
