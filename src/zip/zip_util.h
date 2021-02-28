#ifndef ODR_ZIP_UTIL_H
#define ODR_ZIP_UTIL_H

#include <abstract/file.h>
#include <chrono>
#include <istream>
#include <memory>
#include <miniz.h>
#include <string>

namespace odr::common {
class MemoryFile;
class DiscFile;
} // namespace odr::common

namespace odr::zip::util {

class Archive final {
public:
  explicit Archive(const std::shared_ptr<common::MemoryFile> &file);
  explicit Archive(const std::shared_ptr<common::DiscFile> &file);
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
  [[nodiscard]] std::unique_ptr<std::istream> read() const final;

private:
  std::shared_ptr<Archive> m_archive;
  std::uint32_t m_index;
};

bool append_file(mz_zip_archive &archive, const std::string &path,
                 std::istream &istream, std::size_t size,
                 const std::time_t &time, const std::string &comment,
                 std::uint32_t level_and_flags);

} // namespace odr::zip::util

#endif // ODR_ZIP_UTIL_H
