#ifndef ODR_INTERNAL_COMMON_TEMPORARY_FILE_H
#define ODR_INTERNAL_COMMON_TEMPORARY_FILE_H

#include <functional>
#include <odr/internal/common/file.hpp>

namespace odr::internal::common {

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

class TemporaryDiskFileFactory final {
public:
  using RandomFileNameGenerator = std::function<std::string()>;

  static const TemporaryDiskFileFactory &system_default();
  static RandomFileNameGenerator default_random_file_name_generator();

  explicit TemporaryDiskFileFactory(
      common::Path directory,
      RandomFileNameGenerator random_file_name_generator =
          default_random_file_name_generator());

  TemporaryDiskFile copy(const abstract::File &file) const;
  TemporaryDiskFile copy(std::istream &in) const;

private:
  common::Path m_directory;
  RandomFileNameGenerator m_random_file_name_generator;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_TEMPORARY_FILE_H
