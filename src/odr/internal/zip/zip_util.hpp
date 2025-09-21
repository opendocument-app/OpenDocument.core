#pragma once

#include <odr/internal/abstract/file.hpp>

#include <chrono>
#include <istream>
#include <memory>
#include <mutex>
#include <string>

#include <miniz/miniz.h>
#include <miniz/miniz_zip.h>

namespace odr::internal {
class RelPath;
class MemoryFile;
class DiskFile;
} // namespace odr::internal

namespace odr::internal::zip::util {

enum class Method {
  UNSUPPORTED,
  STORED,
  DEFLATED,
};

class Archive final : public std::enable_shared_from_this<Archive> {
public:
  explicit Archive(const std::shared_ptr<MemoryFile> &file);
  explicit Archive(const std::shared_ptr<DiskFile> &file);
  ~Archive();

  [[nodiscard]] std::mutex &mutex() const;
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
  std::unique_ptr<std::istream> m_stream;

  mutable std::mutex m_mutex;
  mutable mz_zip_archive m_zip{};

  explicit Archive(std::shared_ptr<abstract::File> file);
};

void open_from_file(mz_zip_archive &archive, const abstract::File &file,
                    std::istream &stream);

bool append_file(mz_zip_archive &archive, const std::string &path,
                 std::istream &istream, std::size_t size,
                 const std::time_t &time, const std::string &comment,
                 std::uint32_t level_and_flags);

} // namespace odr::internal::zip::util
