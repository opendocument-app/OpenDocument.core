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

class DiscFile : public abstract::File {
public:
  explicit DiscFile(const char *path);
  explicit DiscFile(const std::string &path);
  explicit DiscFile(common::Path path);

  [[nodiscard]] FileLocation location() const noexcept final;

  [[nodiscard]] std::size_t size() const final;

  [[nodiscard]] common::Path path() const;
  [[nodiscard]] std::unique_ptr<std::istream> read() const final;

private:
  common::Path m_path;
};

class TemporaryDiscFile final : public DiscFile {
public:
  explicit TemporaryDiscFile(const char *path);
  explicit TemporaryDiscFile(std::string path);
  explicit TemporaryDiscFile(common::Path path);
  TemporaryDiscFile(const TemporaryDiscFile &);
  TemporaryDiscFile(TemporaryDiscFile &&) noexcept;
  ~TemporaryDiscFile() override;
  TemporaryDiscFile &operator=(const TemporaryDiscFile &);
  TemporaryDiscFile &operator=(TemporaryDiscFile &&) noexcept;
};

class MemoryFile final : public abstract::File {
public:
  explicit MemoryFile(std::string data);
  explicit MemoryFile(const abstract::File &file);

  [[nodiscard]] FileLocation location() const noexcept final;

  [[nodiscard]] std::size_t size() const final;

  [[nodiscard]] const std::string &content() const;
  [[nodiscard]] std::unique_ptr<std::istream> read() const final;

private:
  std::string m_data;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_FILE_H
