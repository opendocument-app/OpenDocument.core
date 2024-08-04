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

class Archive final : public std::enable_shared_from_this<Archive> {
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
    Entry(const Entry &) = default;
    Entry(Entry &&) noexcept = default;
    Entry(const Archive &parent, const impl::CompoundFileEntry &entry)
        : m_parent{&parent}, m_entry{&entry}, m_path{"/"} {}
    Entry(const Archive &parent, const impl::CompoundFileEntry &entry,
          const common::Path &parent_path)
        : m_parent{&parent}, m_entry{&entry}, m_path{parent_path.join(name())} {
    }
    ~Entry() = default;
    Entry &operator=(const Entry &) = default;
    Entry &operator=(Entry &&) noexcept = default;

    bool operator==(const Entry &other) const {
      return m_entry == other.m_entry;
    }
    bool operator!=(const Entry &other) const {
      return m_entry != other.m_entry;
    }

    [[nodiscard]] bool is_file() const;
    [[nodiscard]] bool is_directory() const;
    [[nodiscard]] common::Path path() const;
    [[nodiscard]] std::unique_ptr<abstract::File> file() const;

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

    Iterator() = default;
    Iterator(const Iterator &) = default;
    Iterator(Iterator &&) noexcept = default;
    Iterator(const Archive &parent, const impl::CompoundFileEntry &entry)
        : m_entry{Entry(parent, entry)} {
      dig_left_();
    }
    Iterator(const Archive &parent, const impl::CompoundFileEntry &entry,
             const common::Path &parent_path)
        : m_entry{Entry(parent, entry, parent_path)} {
      dig_left_();
    }
    ~Iterator() = default;
    Iterator &operator=(const Iterator &) = default;
    Iterator &operator=(Iterator &&) noexcept = default;

    [[nodiscard]] reference operator*() const { return *m_entry; }
    [[nodiscard]] pointer operator->() const { return &*m_entry; }

    [[nodiscard]] bool operator==(const Iterator &other) const {
      return m_entry == other.m_entry;
    }
    [[nodiscard]] bool operator!=(const Iterator &other) const {
      return m_entry != other.m_entry;
    }

    Iterator &operator++() {
      next_();
      return *this;
    }
    Iterator operator++(int) {
      Iterator tmp = *this;
      ++(*this);
      return tmp;
    }

  private:
    std::optional<Entry> m_entry;
    std::vector<Entry> m_ancestors;
    std::vector<Entry> m_directories;

    void dig_left_();
    void next_();
    void next_flat_();
  };

private:
  std::shared_ptr<abstract::File> m_file;
  impl::CompoundFileReader m_cfb;
};

} // namespace odr::internal::cfb::util

#endif // ODR_INTERNAL_CFB_UTIL_HPP
