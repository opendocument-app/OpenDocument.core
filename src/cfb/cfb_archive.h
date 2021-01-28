#ifndef ODR_CFB_ARCHIVE_H
#define ODR_CFB_ARCHIVE_H

#include <cfb/cfb_impl.h>
#include <common/file.h>
#include <common/path.h>

namespace odr::cfb {

class ReadonlyCfbArchive final {
public:
  explicit ReadonlyCfbArchive(std::shared_ptr<common::MemoryFile> file);

  class Iterator;

  [[nodiscard]] Iterator begin() const;
  [[nodiscard]] Iterator end() const;

  [[nodiscard]] Iterator find(const common::Path &path) const;

  class Entry {
  public:
    Entry(const impl::CompoundFileReader &reader,
          const impl::CompoundFileEntry *entry, common::Path path);

    [[nodiscard]] bool is_file() const;
    [[nodiscard]] bool is_directory() const;
    [[nodiscard]] common::Path path() const;
    [[nodiscard]] std::unique_ptr<abstract::File> file() const;
    [[nodiscard]] std::unique_ptr<abstract::File>
    file(std::shared_ptr<ReadonlyCfbArchive> persist) const;

  private:
    const impl::CompoundFileReader &m_reader;
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

    Iterator(const impl::CompoundFileReader &reader,
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
  std::shared_ptr<common::MemoryFile> m_file;
  impl::CompoundFileReader m_reader;
};

} // namespace odr::cfb

#endif // ODR_CFB_ARCHIVE_H
