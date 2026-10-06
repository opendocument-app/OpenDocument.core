#pragma once

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>

#include <functional>

namespace odr::internal {

/// Owns the path it names and removes it on destruction, hence move-only.
class TemporaryDiskFile final : public DiskFile {
public:
  explicit TemporaryDiskFile(const char *path);
  explicit TemporaryDiskFile(const std::string &path);
  explicit TemporaryDiskFile(AbsPath path);
  TemporaryDiskFile(const TemporaryDiskFile &) = delete;
  TemporaryDiskFile(TemporaryDiskFile &&) noexcept;
  ~TemporaryDiskFile() override;
  TemporaryDiskFile &operator=(const TemporaryDiskFile &) = delete;
  TemporaryDiskFile &operator=(TemporaryDiskFile &&) noexcept;

private:
  bool m_owns_path{true}; ///< cleared by a move, so only one owner removes
};

class TemporaryDiskFileFactory final {
public:
  using RandomFileNameGenerator = std::function<std::string()>;

  static const TemporaryDiskFileFactory &system_default();
  static RandomFileNameGenerator default_random_file_name_generator();

  explicit TemporaryDiskFileFactory(
      AbsPath directory, RandomFileNameGenerator random_file_name_generator =
                             default_random_file_name_generator());

  /// Private to the owner, because a copy can hold decrypted content.
  [[nodiscard]] TemporaryDiskFile copy(const abstract::File &file) const;
  [[nodiscard]] TemporaryDiskFile copy(std::istream &in) const;
  /// With the permissions any new file in the directory gets.
  [[nodiscard]] TemporaryDiskFile
  create(const std::function<void(std::ostream &)> &write) const;

private:
  [[nodiscard]] TemporaryDiskFile
  create_(const std::function<void(std::ostream &)> &write,
          bool private_to_owner) const;

  AbsPath m_directory;
  RandomFileNameGenerator m_random_file_name_generator;
};

} // namespace odr::internal
