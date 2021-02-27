#ifndef ODR_COMMON_FILESYSTEM_H
#define ODR_COMMON_FILESYSTEM_H

#include <abstract/filesystem.h>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace odr::common {

class SystemFilesystem final : public abstract::Filesystem {
public:
  explicit SystemFilesystem(Path root);

  [[nodiscard]] bool exists(Path path) const final;
  [[nodiscard]] bool is_file(Path path) const final;
  [[nodiscard]] bool is_directory(Path path) const final;

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(Path path) const final;

  [[nodiscard]] std::shared_ptr<abstract::File> open(Path path) const final;

  std::unique_ptr<std::ostream> create_file(Path path) final;
  bool create_directory(Path path) final;

  bool remove(Path path) final;
  bool copy(Path from, Path to) final;
  std::shared_ptr<abstract::File> copy(const abstract::File &from,
                                       Path to) final;
  std::shared_ptr<abstract::File> copy(std::shared_ptr<abstract::File> from,
                                       Path to) final;
  bool move(Path from, Path to) final;

private:
  Path m_root;

  [[nodiscard]] Path to_system_path_(const Path &path) const;
};

class VirtualFilesystem final : public abstract::Filesystem {
public:
  [[nodiscard]] bool exists(Path path) const final;
  [[nodiscard]] bool is_file(Path path) const final;
  [[nodiscard]] bool is_directory(Path path) const final;

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(Path path) const final;

  [[nodiscard]] std::shared_ptr<abstract::File> open(Path path) const final;

  std::unique_ptr<std::ostream> create_file(Path path) final;
  bool create_directory(Path path) final;

  bool remove(Path path) final;
  bool copy(Path from, Path to) final;
  std::shared_ptr<abstract::File> copy(const abstract::File &from,
                                       Path to) final;
  std::shared_ptr<abstract::File> copy(std::shared_ptr<abstract::File> from,
                                       Path to) final;
  bool move(Path from, Path to) final;

private:
  // TODO consider `const abstract::File`
  std::map<Path, std::shared_ptr<abstract::File>> m_files;
};

} // namespace odr::common

#endif // ODR_COMMON_FILESYSTEM_H
