#ifndef ODR_INTERNAL_ZIP_UTIL_H
#define ODR_INTERNAL_ZIP_UTIL_H

#include <chrono>
#include <cstdint>
#include <istream>
#include <memory>
#include <miniz/miniz.h>
#include <miniz/miniz_zip.h>
#include <odr/file.hpp>
#include <odr/internal/abstract/file.hpp>
#include <string>

namespace odr::internal::common {
class MemoryFile;
class DiskFile;
} // namespace odr::internal::common

namespace odr::internal::zip::util {

class Archive final {
public:
  explicit Archive(const std::shared_ptr<common::MemoryFile> &file);
  explicit Archive(const std::shared_ptr<common::DiskFile> &file);
  Archive(const Archive &);
  Archive(Archive &&) noexcept;
  ~Archive();
  Archive &operator=(const Archive &);
  Archive &operator=(Archive &&) noexcept;

  [[nodiscard]] mz_zip_archive *zip() const;

  [[nodiscard]] std::shared_ptr<abstract::File> file() const;

private:
  mutable mz_zip_archive m_zip{};
  std::shared_ptr<abstract::File> m_file;
  std::unique_ptr<std::istream> m_data;

  explicit Archive(std::shared_ptr<abstract::File> file);

  void init_();
};

class FileInZip final : public abstract::File {
public:
  FileInZip(std::shared_ptr<Archive> archive, std::uint32_t index);

  [[nodiscard]] FileLocation location() const noexcept final;
  [[nodiscard]] std::size_t size() const final;

  [[nodiscard]] std::optional<common::Path> disk_path() const final;
  [[nodiscard]] const char *memory_data() const final;

  [[nodiscard]] std::unique_ptr<std::istream> stream() const final;

private:
  std::shared_ptr<Archive> m_archive;
  std::uint32_t m_index;
};

bool append_file(mz_zip_archive &archive, const std::string &path,
                 std::istream &istream, std::size_t size,
                 const std::time_t &time, const std::string &comment,
                 std::uint32_t level_and_flags);

} // namespace odr::internal::zip::util

#endif // ODR_INTERNAL_ZIP_UTIL_H
