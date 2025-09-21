#pragma once

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/path.hpp>

#include <iosfwd>
#include <memory>
#include <string>

namespace odr {
enum class FileLocation;
}

namespace odr::internal {

class DiskFile : public abstract::File {
public:
  explicit DiskFile(const char *path);
  explicit DiskFile(const std::string &path);
  explicit DiskFile(AbsPath path);

  [[nodiscard]] FileLocation location() const noexcept final;
  [[nodiscard]] std::size_t size() const final;

  [[nodiscard]] std::optional<AbsPath> disk_path() const final;
  [[nodiscard]] const char *memory_data() const final;

  [[nodiscard]] std::unique_ptr<std::istream> stream() const final;

private:
  AbsPath m_path;
};

class MemoryFile final : public abstract::File {
public:
  explicit MemoryFile(std::string data);
  explicit MemoryFile(const File &file);

  [[nodiscard]] FileLocation location() const noexcept override;
  [[nodiscard]] std::size_t size() const override;

  [[nodiscard]] std::optional<AbsPath> disk_path() const override;
  [[nodiscard]] const char *memory_data() const override;

  [[nodiscard]] std::unique_ptr<std::istream> stream() const override;

  [[nodiscard]] const std::string &content() const;

private:
  std::string m_data;
};

} // namespace odr::internal
