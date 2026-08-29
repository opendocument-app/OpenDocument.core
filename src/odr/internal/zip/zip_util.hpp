#pragma once

#include <odr/internal/abstract/file.hpp>

#include <chrono>
#include <istream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <miniz/miniz.h>
#include <miniz/miniz_zip.h>

namespace odr::internal {
class RelPath;
} // namespace odr::internal

namespace odr::internal::zip::util {

enum class Method {
  UNSUPPORTED,
  STORED,
  DEFLATED,
};

/// Re-entrant source for `mz_zip_archive::m_pRead`, which reads by absolute
/// offset. The lock covers the stream free list, never a read.
class ReadSource final {
public:
  explicit ReadSource(std::shared_ptr<abstract::File> file);

  [[nodiscard]] std::size_t read(std::uint64_t offset, void *buffer,
                                 std::size_t size) const;

private:
  std::shared_ptr<abstract::File> m_file;
  std::optional<std::string_view> m_memory;

  mutable std::mutex m_mutex;
  mutable std::vector<std::unique_ptr<std::istream>> m_streams;
};

/// Entries can be read concurrently. `mz_zip_archive::m_last_error` cannot, and
/// is never read.
class Archive final : public std::enable_shared_from_this<Archive> {
public:
  explicit Archive(std::shared_ptr<abstract::File> file);
  ~Archive();

  [[nodiscard]] mz_zip_archive *zip() const;

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept;

  class Iterator;

  [[nodiscard]] Iterator begin() const;
  [[nodiscard]] Iterator end() const;

  [[nodiscard]] Iterator find(const RelPath &path) const;

  class Entry {
  public:
    Entry(const Entry &) = default;
    Entry(Entry &&) noexcept = default;
    Entry(const Archive &archive, const std::uint32_t index)
        : m_archive{&archive}, m_index{index} {}
    ~Entry() = default;
    Entry &operator=(const Entry &) = default;
    Entry &operator=(Entry &&) noexcept = default;

    [[nodiscard]] bool operator==(const Entry &other) const {
      return m_index == other.m_index;
    }

    [[nodiscard]] bool is_file() const;
    [[nodiscard]] bool is_directory() const;
    [[nodiscard]] RelPath path() const;
    [[nodiscard]] Method method() const;
    [[nodiscard]] std::shared_ptr<abstract::File> file() const;

  private:
    const Archive *m_archive;
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

    Iterator(const Archive &zip, const std::uint32_t index)
        : m_entry{zip, index} {}

    [[nodiscard]] reference operator*() const { return m_entry; }
    [[nodiscard]] pointer operator->() const { return &m_entry; }

    [[nodiscard]] bool operator==(const Iterator &other) const {
      return m_entry == other.m_entry;
    }

    Iterator &operator++() {
      m_entry.m_index++;
      return *this;
    }
    Iterator operator++(int) {
      const Iterator tmp = *this;
      ++*this;
      return tmp;
    }

  private:
    Entry m_entry;
  };

private:
  std::shared_ptr<abstract::File> m_file;
  std::unique_ptr<ReadSource> m_source;

  mutable mz_zip_archive m_zip{};
};

void open_from_file(mz_zip_archive &archive, const abstract::File &file,
                    ReadSource &source);

/// `archive`'s write callback has to honour the offset it is given — local
/// headers are rewritten with the entry size.
bool append_file(mz_zip_archive &archive, const std::string &path,
                 std::istream &istream, std::size_t size,
                 const std::time_t &time, const std::string &comment,
                 std::uint32_t level_and_flags);

} // namespace odr::internal::zip::util
