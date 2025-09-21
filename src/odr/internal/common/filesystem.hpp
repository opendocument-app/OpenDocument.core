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

  [[nodiscard]] bool exists(const AbsPath &path) const override;
  [[nodiscard]] bool is_file(const AbsPath &path) const override;
  [[nodiscard]] bool is_directory(const AbsPath &path) const override;

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(const AbsPath &path) const override;

  [[nodiscard]] std::shared_ptr<abstract::File>
  open(const AbsPath &path) const override;

  std::unique_ptr<std::ostream> create_file(const AbsPath &path) override;
  bool create_directory(const AbsPath &path) override;

  bool remove(const AbsPath &path) override;
  bool copy(const AbsPath &from, const AbsPath &to) override;
  std::shared_ptr<abstract::File> copy(const abstract::File &from,
                                       const AbsPath &to) override;
  std::shared_ptr<abstract::File> copy(std::shared_ptr<abstract::File> from,
                                       const AbsPath &to) override;
  bool move(const AbsPath &from, const AbsPath &to) override;

private:
  AbsPath m_root;

  [[nodiscard]] AbsPath to_system_path_(const AbsPath &path) const;
};

class VirtualFilesystem final : public abstract::Filesystem {
public:
  [[nodiscard]] bool exists(const AbsPath &path) const override;
  [[nodiscard]] bool is_file(const AbsPath &path) const override;
  [[nodiscard]] bool is_directory(const AbsPath &path) const override;

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(const AbsPath &path) const override;

  [[nodiscard]] std::shared_ptr<abstract::File>
  open(const AbsPath &path) const override;

  std::unique_ptr<std::ostream> create_file(const AbsPath &path) override;
  bool create_directory(const AbsPath &path) override;

  bool remove(const AbsPath &path) override;
  bool copy(const AbsPath &from, const AbsPath &to) override;
  std::shared_ptr<abstract::File> copy(const abstract::File &from,
                                       const AbsPath &to) override;
  std::shared_ptr<abstract::File> copy(std::shared_ptr<abstract::File> from,
                                       const AbsPath &to) override;
  bool move(const AbsPath &from, const AbsPath &to) override;

private:
  // TODO consider `const abstract::File`
  std::map<AbsPath, std::shared_ptr<abstract::File>> m_files;
};

} // namespace odr::internal
