#pragma once

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>

#include <iosfwd>
#include <map>
#include <memory>

namespace odr::internal::abstract {
class File;
} // namespace odr::internal::abstract

namespace odr::internal {

class SystemFilesystem final : public abstract::Filesystem {
public:
  explicit SystemFilesystem(AbsPath root);

  [[nodiscard]] bool exists(const AbsPath &path) const final;
  [[nodiscard]] bool is_file(const AbsPath &path) const final;
  [[nodiscard]] bool is_directory(const AbsPath &path) const final;

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(const AbsPath &path) const final;

  [[nodiscard]] std::shared_ptr<abstract::File>
  open(const AbsPath &path) const final;

  std::unique_ptr<std::ostream> create_file(const AbsPath &path) final;
  bool create_directory(const AbsPath &path) final;

  bool remove(const AbsPath &path) final;
  bool copy(const AbsPath &from, const AbsPath &to) final;
  std::shared_ptr<abstract::File> copy(const abstract::File &from,
                                       const AbsPath &to) final;
  std::shared_ptr<abstract::File> copy(std::shared_ptr<abstract::File> from,
                                       const AbsPath &to) final;
  bool move(const AbsPath &from, const AbsPath &to) final;

private:
  AbsPath m_root;

  [[nodiscard]] AbsPath to_system_path_(const AbsPath &path) const;
};

class VirtualFilesystem final : public abstract::Filesystem {
public:
  [[nodiscard]] bool exists(const AbsPath &path) const final;
  [[nodiscard]] bool is_file(const AbsPath &path) const final;
  [[nodiscard]] bool is_directory(const AbsPath &path) const final;

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(const AbsPath &path) const final;

  [[nodiscard]] std::shared_ptr<abstract::File>
  open(const AbsPath &path) const final;

  std::unique_ptr<std::ostream> create_file(const AbsPath &path) final;
  bool create_directory(const AbsPath &path) final;

  bool remove(const AbsPath &path) final;
  bool copy(const AbsPath &from, const AbsPath &to) final;
  std::shared_ptr<abstract::File> copy(const abstract::File &from,
                                       const AbsPath &to) final;
  std::shared_ptr<abstract::File> copy(std::shared_ptr<abstract::File> from,
                                       const AbsPath &to) final;
  bool move(const AbsPath &from, const AbsPath &to) final;

private:
  // TODO consider `const abstract::File`
  std::map<AbsPath, std::shared_ptr<abstract::File>> m_files;
};

} // namespace odr::internal
