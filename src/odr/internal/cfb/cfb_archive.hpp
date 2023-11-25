#ifndef ODR_INTERNAL_CFB_ARCHIVE_H
#define ODR_INTERNAL_CFB_ARCHIVE_H

#include <odr/internal/cfb/cfb_impl.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>

#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace odr::internal::abstract {
class File;
} // namespace odr::internal::abstract

namespace odr::internal::cfb::impl {
struct CompoundFileEntry;
} // namespace odr::internal::cfb::impl

namespace odr::internal::common {
class MemoryFile;
} // namespace odr::internal::common

namespace odr::internal::cfb::util {
class Archive;
}

namespace odr::internal::cfb {

class ReadonlyCfbArchive final {
public:
  explicit ReadonlyCfbArchive(const std::shared_ptr<common::MemoryFile> &file);

  class Iterator;

  [[nodiscard]] Iterator begin() const;
  [[nodiscard]] Iterator end() const;

  [[nodiscard]] Iterator find(const common::Path &path) const;

  class Entry {
  public:
    Entry(const ReadonlyCfbArchive &parent,
          const impl::CompoundFileEntry &entry);
    Entry(const ReadonlyCfbArchive &parent,
          const impl::CompoundFileEntry &entry,
          const common::Path &parent_path);

    bool operator==(const Entry &other) const;
    bool operator!=(const Entry &other) const;

    [[nodiscard]] bool is_file() const;
    [[nodiscard]] bool is_directory() const;
    [[nodiscard]] common::Path path() const;
    [[nodiscard]] std::unique_ptr<abstract::File> file() const;

    [[nodiscard]] std::string name() const;
    [[nodiscard]] std::optional<Entry> left() const;
    [[nodiscard]] std::optional<Entry> right() const;
    [[nodiscard]] std::optional<Entry> child() const;

  private:
    const ReadonlyCfbArchive *m_parent;
    const impl::CompoundFileEntry *m_entry;
    common::Path m_path;

    friend Iterator;
  };

  class Iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = Entry;
    using pointer = const Entry *;
    using reference = const Entry &;

    Iterator();
    Iterator(const ReadonlyCfbArchive &parent,
             const impl::CompoundFileEntry &entry);
    Iterator(const ReadonlyCfbArchive &parent,
             const impl::CompoundFileEntry &entry,
             const common::Path &parent_path);

    reference operator*() const;
    pointer operator->() const;

    bool operator==(const Iterator &other) const;
    bool operator!=(const Iterator &other) const;

    Iterator &operator++();
    Iterator operator++(int);

  private:
    std::optional<Entry> m_entry;
    std::vector<Entry> m_ancestors;
    std::vector<Entry> m_directories;

    void dig_left_();
    void next_();
    void next_flat_();
  };

private:
  std::shared_ptr<util::Archive> m_cfb;
};

} // namespace odr::internal::cfb

#endif // ODR_INTERNAL_CFB_ARCHIVE_H
