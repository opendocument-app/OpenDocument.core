#ifndef ODR_INTERNAL_COMMON_ARCHIVE_H
#define ODR_INTERNAL_COMMON_ARCHIVE_H

#include <internal/abstract/archive.h>
#include <internal/abstract/file.h>
#include <internal/common/filesystem.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr::internal::common {

template <typename Impl> class Archive : public abstract::Archive {
public:
  explicit Archive(const Impl &impl)
      : m_filesystem{std::make_shared<VirtualFilesystem>()} {
    fill_(impl);
  }

  [[nodiscard]] std::shared_ptr<abstract::Filesystem> filesystem() const final {
    return m_filesystem;
  }

  void save(const common::Path &path) const final {
    throw UnsupportedOperation();
  }

private:
  // TODO a different `VirtualFilesystem` would be necessary for saving
  std::shared_ptr<VirtualFilesystem> m_filesystem;

  void fill_(const Impl &impl) {
    for (auto &&e : impl) {
      if (e.is_directory()) {
        m_filesystem->create_directory(e.path());
      } else if (e.is_file()) {
        // TODO the file must persist `impl`; how can we enforce that?
        m_filesystem->copy(e.file(), e.path());
      }
    }
  }
};

// TODO `ArchiveFile` should use readonly interfaces
template <typename Impl> class ArchiveFile : public abstract::ArchiveFile {
public:
  template <typename... Args>
  ArchiveFile(Args &&... args) : m_impl{std::forward<Args>(args)...} {}
  explicit ArchiveFile(Impl impl) : m_impl{std::move(impl)} {}

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final {
    return {}; // TODO
  }

  [[nodiscard]] FileType file_type() const noexcept final {
    return FileType::UNKNOWN; // TODO
  }

  [[nodiscard]] FileMeta file_meta() const noexcept final {
    return {}; // TODO
  }

  [[nodiscard]] std::shared_ptr<abstract::Archive> archive() const final {
    return std::make_shared<Archive<Impl>>(m_impl);
  }

private:
  Impl m_impl;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_ARCHIVE_H
