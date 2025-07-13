#pragma once

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>

#include <functional>

namespace odr::internal {

class TemporaryDiskFile final : public DiskFile {
public:
  explicit TemporaryDiskFile(const char *path);
  explicit TemporaryDiskFile(const std::string &path);
  explicit TemporaryDiskFile(const Path &path);
  TemporaryDiskFile(const TemporaryDiskFile &);
  TemporaryDiskFile(TemporaryDiskFile &&) noexcept;
  ~TemporaryDiskFile() override;
  TemporaryDiskFile &operator=(const TemporaryDiskFile &);
  TemporaryDiskFile &operator=(TemporaryDiskFile &&) noexcept;
};

class TemporaryDiskFileFactory final {
public:
  using RandomFileNameGenerator = std::function<std::string()>;

  static const TemporaryDiskFileFactory &system_default();
  static RandomFileNameGenerator default_random_file_name_generator();

  explicit TemporaryDiskFileFactory(
      Path directory, RandomFileNameGenerator random_file_name_generator =
                          default_random_file_name_generator());

  [[nodiscard]] TemporaryDiskFile copy(const abstract::File &file) const;
  [[nodiscard]] TemporaryDiskFile copy(std::istream &in) const;

private:
  Path m_directory;
  RandomFileNameGenerator m_random_file_name_generator;
};

} // namespace odr::internal
