#ifndef ODR_INTERNAL_ZIP_UTIL_HPP
#define ODR_INTERNAL_ZIP_UTIL_HPP

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>

#include <chrono>
#include <cstdint>
#include <istream>
#include <memory>
#include <string>

#include <miniz/miniz.h>
#include <miniz/miniz_zip.h>

namespace odr::internal::common {
class MemoryFile;
class DiskFile;
} // namespace odr::internal::common

namespace odr::internal::zip::util {

enum class Method {
  UNSUPPORTED,
  STORED,
  DEFLATED,
};

class Archive final : public std::enable_shared_from_this<Archive> {
public:
  explicit Archive(const std::shared_ptr<common::MemoryFile> &file);
  explicit Archive(const std::shared_ptr<common::DiskFile> &file);
  Archive(const Archive &);
  Archive(Archive &&) noexcept;
  ~Archive();
  Archive &operator=(const Archive &);
  Archive &operator=(Archive &&) noexcept;

  [[nodiscard]] const mz_zip_archive *zip() const;

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept;

  class Iterator;

  [[nodiscard]] Iterator begin() const;
  [[nodiscard]] Iterator end() const;

  [[nodiscard]] Iterator find(const common::Path &path) const;

  class Entry {
  public:
    Entry(const Archive &parent, std::uint32_t index);

    [[nodiscard]] bool is_file() const;
    [[nodiscard]] bool is_directory() const;
    [[nodiscard]] common::Path path() const;
    [[nodiscard]] Method method() const;
    [[nodiscard]] std::shared_ptr<abstract::File> file() const;

  private:
    const Archive &m_parent;
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

    Iterator(const Archive &zip, std::uint32_t index);

    reference operator*() const;
    pointer operator->() const;

    bool operator==(const Iterator &other) const;
    bool operator!=(const Iterator &other) const;

    Iterator &operator++();
    Iterator operator++(int);

  private:
    Entry m_entry;
  };

private:
  std::shared_ptr<abstract::File> m_file;
  std::unique_ptr<std::istream> m_stream;
  mz_zip_archive m_zip{};

  explicit Archive(std::shared_ptr<abstract::File> file);
};

void open_from_file(mz_zip_archive &archive, const abstract::File &file,
                    std::istream &stream);

bool append_file(mz_zip_archive &archive, const std::string &path,
                 std::istream &istream, std::size_t size,
                 const std::time_t &time, const std::string &comment,
                 std::uint32_t level_and_flags);

} // namespace odr::internal::zip::util

#endif // ODR_INTERNAL_ZIP_UTIL_HPP
