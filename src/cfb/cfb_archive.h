#ifndef ODR_CFB_ARCHIVE_H
#define ODR_CFB_ARCHIVE_H

#include <cfb/cfb_impl.h>
#include <common/file.h>
#include <common/path.h>
#include <vector>

namespace odr::cfb {
namespace util {
class Archive;
}

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
          const impl::CompoundFileEntry *entry, common::Path path);

    [[nodiscard]] bool is_file() const;
    [[nodiscard]] bool is_directory() const;
    [[nodiscard]] common::Path path() const;
    [[nodiscard]] std::unique_ptr<abstract::File> file() const;

  private:
    const ReadonlyCfbArchive &m_parent;
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

    Iterator(const ReadonlyCfbArchive &parent,
             const impl::CompoundFileEntry *entry, common::Path path);

    reference operator*() const;
    pointer operator->() const;

    bool operator==(const Iterator &other) const;
    bool operator!=(const Iterator &other) const;

    Iterator &operator++();
    Iterator operator++(int);

  private:
    std::vector<const impl::CompoundFileEntry *> m_ancestors;
    std::vector<const impl::CompoundFileEntry *> m_directories;
    Entry m_entry;

    void dig_left_();
  };

private:
  std::shared_ptr<util::Archive> m_cfb;
};

} // namespace odr::cfb

#endif // ODR_CFB_ARCHIVE_H
