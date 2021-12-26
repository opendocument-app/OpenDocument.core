#ifndef ODR_INTERNAL_COMMON_FILE_H
#define ODR_INTERNAL_COMMON_FILE_H

#include <internal/abstract/file.h>
#include <internal/common/path.h>
#include <iosfwd>
#include <memory>
#include <string>

namespace odr {
enum class FileLocation;
}

namespace odr::internal::common {

class DiskFile : public abstract::DiskFile {
public:
  explicit DiskFile(const char *path);
  explicit DiskFile(const std::string &path);
  explicit DiskFile(common::Path path);

  [[nodiscard]] std::size_t size() const final;
  [[nodiscard]] std::unique_ptr<std::istream> stream() const final;

  [[nodiscard]] common::Path path() const;

private:
  common::Path m_path;
};

class TemporaryDiskFile final : public DiskFile {
public:
  explicit TemporaryDiskFile(const char *path);
  explicit TemporaryDiskFile(std::string path);
  explicit TemporaryDiskFile(common::Path path);
  TemporaryDiskFile(const TemporaryDiskFile &);
  TemporaryDiskFile(TemporaryDiskFile &&) noexcept;
  ~TemporaryDiskFile() override;
  TemporaryDiskFile &operator=(const TemporaryDiskFile &);
  TemporaryDiskFile &operator=(TemporaryDiskFile &&) noexcept;
};

class MemoryFile final : public abstract::MemoryFile {
public:
  explicit MemoryFile(std::string data);
  explicit MemoryFile(const abstract::File &file);

  [[nodiscard]] std::size_t size() const final;
  [[nodiscard]] std::unique_ptr<std::istream> stream() const final;

  [[nodiscard]] const char *data() const final;
  [[nodiscard]] const std::string &content() const;

private:
  std::string m_data;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_FILE_H
