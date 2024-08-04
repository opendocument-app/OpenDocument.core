#ifndef ODR_INTERNAL_CFB_UTIL_HPP
#define ODR_INTERNAL_CFB_UTIL_HPP

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/cfb/cfb_impl.hpp>
#include <odr/internal/common/path.hpp>

#include <istream>
#include <memory>
#include <string>

namespace odr::internal::common {
class MemoryFile;
} // namespace odr::internal::common

namespace odr::internal::cfb::impl {
class CompoundFileReader;
struct CompoundFileEntry;
} // namespace odr::internal::cfb::impl

namespace odr::internal::cfb::util {

class Archive final {
public:
  explicit Archive(const std::shared_ptr<common::MemoryFile> &file);

  [[nodiscard]] const impl::CompoundFileReader &cfb() const;

  [[nodiscard]] std::shared_ptr<abstract::File> file() const;

  class Iterator;

  [[nodiscard]] Iterator begin() const;
  [[nodiscard]] Iterator end() const;

  [[nodiscard]] Iterator find(const common::Path &path) const;

  class Entry {
  public:
    Entry(const Archive &parent, const impl::CompoundFileEntry &entry);
    Entry(const Archive &parent, const impl::CompoundFileEntry &entry,
          const common::Path &parent_path);

    bool operator==(const Entry &other) const;
    bool operator!=(const Entry &other) const;

    [[nodiscard]] bool is_file() const;
    [[nodiscard]] bool is_directory() const;
    [[nodiscard]] common::Path path() const;
    [[nodiscard]] std::unique_ptr<abstract::File>
    file(std::shared_ptr<Archive> archive) const;

    [[nodiscard]] std::string name() const;
    [[nodiscard]] std::optional<Entry> left() const;
    [[nodiscard]] std::optional<Entry> right() const;
    [[nodiscard]] std::optional<Entry> child() const;

  private:
    const Archive *m_parent;
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
    Iterator(const Archive &parent, const impl::CompoundFileEntry &entry);
    Iterator(const Archive &parent, const impl::CompoundFileEntry &entry,
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
  impl::CompoundFileReader m_cfb;
  std::shared_ptr<abstract::File> m_file;
};

} // namespace odr::internal::cfb::util

#endif // ODR_INTERNAL_CFB_UTIL_HPP
