#pragma once

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>

#include <iosfwd>
#include <map>
#include <memory>

namespace odr::internal::abstract {
class File;
} // namespace odr::internal::abstract

namespace odr::internal::common {

class SystemFilesystem final : public abstract::Filesystem {
public:
  explicit SystemFilesystem(Path root);

  [[nodiscard]] bool exists(const Path &path) const final;
  [[nodiscard]] bool is_file(const Path &path) const final;
  [[nodiscard]] bool is_directory(const Path &path) const final;

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(const Path &path) const final;

  [[nodiscard]] std::shared_ptr<abstract::File>
  open(const Path &path) const final;

  std::unique_ptr<std::ostream> create_file(const Path &path) final;
  bool create_directory(const Path &path) final;

  bool remove(const Path &path) final;
  bool copy(const Path &from, const Path &to) final;
  std::shared_ptr<abstract::File> copy(const abstract::File &from,
                                       const Path &to) final;
  std::shared_ptr<abstract::File> copy(std::shared_ptr<abstract::File> from,
                                       const Path &to) final;
  bool move(const Path &from, const Path &to) final;

private:
  Path m_root;

  [[nodiscard]] Path to_system_path_(const Path &path) const;
};

class VirtualFilesystem final : public abstract::Filesystem {
public:
  [[nodiscard]] bool exists(const Path &path) const final;
  [[nodiscard]] bool is_file(const Path &path) const final;
  [[nodiscard]] bool is_directory(const Path &path) const final;

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(const Path &path) const final;

  [[nodiscard]] std::shared_ptr<abstract::File>
  open(const Path &path) const final;

  std::unique_ptr<std::ostream> create_file(const Path &path) final;
  bool create_directory(const Path &path) final;

  bool remove(const Path &path) final;
  bool copy(const Path &from, const Path &to) final;
  std::shared_ptr<abstract::File> copy(const abstract::File &from,
                                       const Path &to) final;
  std::shared_ptr<abstract::File> copy(std::shared_ptr<abstract::File> from,
                                       const Path &to) final;
  bool move(const Path &from, const Path &to) final;

private:
  // TODO consider `const abstract::File`
  std::map<Path, std::shared_ptr<abstract::File>> m_files;
};

} // namespace odr::internal::common
