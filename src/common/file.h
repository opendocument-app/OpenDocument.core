#ifndef ODR_COMMON_FILE_H
#define ODR_COMMON_FILE_H

#include <abstract/file.h>
#include <common/path.h>

namespace odr::common {

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
  ~TemporaryDiscFile() override;
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

} // namespace odr::common

#endif // ODR_COMMON_FILE_H
