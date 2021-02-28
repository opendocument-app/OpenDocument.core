#ifndef ODR_ZIP_ARCHIVE_H
#define ODR_ZIP_ARCHIVE_H

#include <common/path.h>
#include <miniz.h>
#include <vector>

namespace odr::abstract {
class File;
}

namespace odr::zip {

enum class Method {
  UNSUPPORTED,
  STORED,
  DEFLATED,
};

class ReadonlyZipArchive final {
public:
  explicit ReadonlyZipArchive(std::shared_ptr<abstract::File> file);

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
    [[nodiscard]] std::unique_ptr<abstract::File>
    file(std::shared_ptr<ReadonlyZipArchive> persist) const;

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
  mutable mz_zip_archive m_zip{};
  std::shared_ptr<abstract::File> m_file;
  std::unique_ptr<std::istream> m_data;
};

class ZipArchive final {
public:
  ZipArchive();
  explicit ZipArchive(std::shared_ptr<abstract::File> file);
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

} // namespace odr::zip

#endif // ODR_ZIP_ARCHIVE_H
