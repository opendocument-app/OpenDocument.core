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

  class Iterator;

  [[nodiscard]] Iterator begin() const;
  [[nodiscard]] Iterator end() const;

  [[nodiscard]] Iterator find(const AbsPath &path) const;

  class Entry {
  public:
    Entry(const Entry &) = default;
    Entry(Entry &&) noexcept = default;
    Entry(const Archive &archive, const std::uint32_t entry_id,
          const impl::CompoundFileEntry &entry)
        : m_archive{&archive}, m_entry_id{entry_id}, m_entry{entry},
          m_path{"/"} {}
    Entry(const Archive &archive, const std::uint32_t entry_id,
          const impl::CompoundFileEntry &entry, const AbsPath &parent_path)
        : m_archive{&archive}, m_entry_id{entry_id}, m_entry{entry},
          m_path{parent_path.join(RelPath(name()))} {}
    ~Entry() = default;
    Entry &operator=(const Entry &) = default;
    Entry &operator=(Entry &&) noexcept = default;

    bool operator==(const Entry &other) const {
      return m_entry_id == other.m_entry_id;
    }

    [[nodiscard]] bool is_file() const;
    [[nodiscard]] bool is_directory() const;
    [[nodiscard]] AbsPath path() const;
    [[nodiscard]] std::unique_ptr<abstract::File> file() const;

    [[nodiscard]] std::string name() const;
    [[nodiscard]] std::optional<Entry> left() const;
    [[nodiscard]] std::optional<Entry> right() const;
    [[nodiscard]] std::optional<Entry> child() const;

  private:
    const Archive *m_archive{};
    std::uint32_t m_entry_id{};
    impl::CompoundFileEntry m_entry{};
    AbsPath m_path;

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
    Iterator(const Archive &parent, const std::uint32_t entry_id,
             const impl::CompoundFileEntry &entry)
        : m_entry{Entry(parent, entry_id, entry)} {
      dig_left_();
    }
    Iterator(const Archive &parent, const std::uint32_t entry_id,
             const impl::CompoundFileEntry &entry, const AbsPath &parent_path)
        : m_entry{Entry(parent, entry_id, entry, parent_path)} {
      dig_left_();
    }

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

    void dig_left_();
    void next_();
    void next_flat_();
  };

private:
  std::shared_ptr<abstract::File> m_file;
  impl::CompoundFileReader m_cfb;
};

} // namespace odr::internal::cfb::util
