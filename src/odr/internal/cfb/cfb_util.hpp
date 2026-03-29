#pragma once

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/cfb/cfb_impl.hpp>
#include <odr/internal/common/path.hpp>

#include <istream>
#include <memory>
#include <string>

namespace odr::abstract {
class File;
} // namespace odr::abstract

namespace odr::internal::cfb::impl {
class CompoundFileReader;
struct CompoundFileEntry;
} // namespace odr::internal::cfb::impl

namespace odr::internal::cfb::util {

class Archive final : public std::enable_shared_from_this<Archive> {
public:
  explicit Archive(std::shared_ptr<abstract::File> file);

  [[nodiscard]] const impl::CompoundFileReader &cfb() const;
  [[nodiscard]] std::shared_ptr<abstract::File> file() const;

  class Entry;

  Entry root() const;

  class Iterator;

  [[nodiscard]] Iterator begin() const;
  [[nodiscard]] Iterator end() const;

  [[nodiscard]] Iterator find(const RelPath &path) const;

  class Entry {
  public:
    Entry(const Archive &archive, const std::uint32_t entry_id,
          const impl::CompoundFileEntry &entry, RelPath path)
        : m_archive{&archive}, m_entry_id{entry_id}, m_entry{entry},
          m_path{std::move(path)} {}

    bool operator==(const Entry &other) const {
      return m_entry_id == other.m_entry_id;
    }

    [[nodiscard]] bool is_file() const { return m_entry.is_file(); }
    [[nodiscard]] bool is_directory() const { return m_entry.is_directory(); }
    [[nodiscard]] RelPath path() const { return m_path; }
    [[nodiscard]] std::unique_ptr<abstract::File> file() const;

    [[nodiscard]] std::string name() const { return m_entry.get_name(); }
    [[nodiscard]] std::optional<Entry> left() const;
    [[nodiscard]] std::optional<Entry> right() const;
    [[nodiscard]] std::optional<Entry> child() const;

  private:
    const Archive *m_archive{};
    std::uint32_t m_entry_id{impl::NullId};
    impl::CompoundFileEntry m_entry{};
    RelPath m_path;

    friend Iterator;
  };

  class Iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = Entry;
    using pointer = const Entry *;
    using reference = const Entry &;

    static Iterator begin(const Entry &entry) { return Iterator(entry); }
    static Iterator end() { return {}; }

    [[nodiscard]] reference operator*() const { return *m_entry; }
    [[nodiscard]] pointer operator->() const { return &*m_entry; }

    [[nodiscard]] bool operator==(const Iterator &other) const {
      return m_entry == other.m_entry;
    }

    Iterator &operator++() {
      next_();
      return *this;
    }
    Iterator operator++(int) {
      Iterator tmp = *this;
      ++*this;
      return tmp;
    }

  private:
    std::optional<Entry> m_entry;
    std::vector<Entry> m_ancestors;
    std::vector<Entry> m_directories;

    Iterator() = default;
    explicit Iterator(const Entry &root_entry) : m_entry{root_entry} {
      dig_left_();
    }

    void dig_left_();
    void next_();
    void next_flat_();
  };

private:
  std::shared_ptr<abstract::File> m_file;
  impl::CompoundFileReader m_cfb;
};

} // namespace odr::internal::cfb::util
