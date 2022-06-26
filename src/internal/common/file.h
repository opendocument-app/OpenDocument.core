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

class DiskFile : public abstract::File {
public:
  explicit DiskFile(const char *path);
  explicit DiskFile(const std::string &path);
  explicit DiskFile(common::Path path);

  [[nodiscard]] FileLocation location() const noexcept final;
  [[nodiscard]] std::size_t size() const final;

  [[nodiscard]] std::optional<common::Path> disk_path() const final;
  [[nodiscard]] const char *memory_data() const final;

  [[nodiscard]] std::unique_ptr<std::istream> stream() const final;

private:
  common::Path m_path;
};

class MemoryFile final : public abstract::File {
public:
  explicit MemoryFile(std::string data);
  explicit MemoryFile(const abstract::File &file);

  [[nodiscard]] FileLocation location() const noexcept final;
  [[nodiscard]] std::size_t size() const final;

  [[nodiscard]] std::optional<common::Path> disk_path() const final;
  [[nodiscard]] const char *memory_data() const final;

  [[nodiscard]] std::unique_ptr<std::istream> stream() const final;

  [[nodiscard]] const std::string &content() const;

private:
  std::string m_data;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_FILE_H
