#ifndef ODR_ARCHIVE_H
#define ODR_ARCHIVE_H

#include <memory>
#include <optional>

namespace odr::abstract {
class Archive;
class ArchiveEntry;
class ArchiveEntryIterator;
} // namespace odr::abstract

namespace odr {
class File;

class ArchiveEntry;
class ArchiveEntryIterator;

enum class ArchiveEntryType {
  UNKNOWN,
  FILE,
  DIRECTORY,
};

class Archive final {
public:
  Archive();
  explicit Archive(std::shared_ptr<abstract::Archive> impl);

  [[nodiscard]] ArchiveEntryIterator begin() const;
  [[nodiscard]] ArchiveEntryIterator end() const;

  [[nodiscard]] ArchiveEntryIterator find(const std::string &path) const;
  void move(ArchiveEntry entry, const std::string &path) const;
  void remove(ArchiveEntry entry);

  void save(const std::string &path) const;

private:
  std::shared_ptr<abstract::Archive> m_impl;
};

class ArchiveEntry final {
public:
  ArchiveEntry();
  explicit ArchiveEntry(std::shared_ptr<abstract::ArchiveEntry> impl);

  bool operator==(const ArchiveEntry &rhs) const;
  bool operator!=(const ArchiveEntry &rhs) const;
  explicit operator bool() const;

  [[nodiscard]] ArchiveEntryType type() const;
  [[nodiscard]] std::string path() const;

  [[nodiscard]] std::optional<File> open() const;

private:
  std::shared_ptr<abstract::ArchiveEntry> m_impl;

  friend Archive;
};

class ArchiveEntryIterator final {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = ArchiveEntry;
  using difference_type = std::ptrdiff_t;
  using pointer = ArchiveEntry *;
  using reference = ArchiveEntry &;

  explicit ArchiveEntryIterator(
      std::shared_ptr<abstract::ArchiveEntryIterator> impl);

  ArchiveEntryIterator &operator++();
  ArchiveEntryIterator operator++(int) &;
  reference operator*();
  pointer operator->();
  bool operator==(const ArchiveEntryIterator &rhs) const;
  bool operator!=(const ArchiveEntryIterator &rhs) const;

private:
  std::shared_ptr<abstract::ArchiveEntryIterator> m_impl;
  ArchiveEntry m_entry;
};

} // namespace odr

#endif // ODR_ARCHIVE_H
