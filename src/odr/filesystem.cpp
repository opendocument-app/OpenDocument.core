#include <odr/filesystem.hpp>

#include <odr/file.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>

namespace odr {

FileWalker::FileWalker(std::unique_ptr<internal::abstract::FileWalker> impl)
    : m_impl{std::move(impl)} {
  if (m_impl == nullptr) {
    throw std::invalid_argument("impl must not be null");
  }
}

FileWalker::FileWalker(const FileWalker &other)
    : m_impl{other.m_impl->clone()} {}

FileWalker::FileWalker(FileWalker &&other) noexcept = default;

FileWalker::~FileWalker() = default;

FileWalker &FileWalker::operator=(const FileWalker &other) {
  if (this == &other) {
    return *this;
  }
  m_impl = other.m_impl->clone();
  return *this;
}

FileWalker &FileWalker::operator=(FileWalker &&) noexcept = default;

bool FileWalker::end() const { return m_impl->end(); }

std::uint32_t FileWalker::depth() const { return m_impl->depth(); }

std::string FileWalker::path() const { return m_impl->path().string(); }

bool FileWalker::is_file() const { return m_impl->is_file(); }

bool FileWalker::is_directory() const { return m_impl->is_directory(); }

void FileWalker::pop() const { m_impl->pop(); }

void FileWalker::next() const { m_impl->next(); }

void FileWalker::flat_next() const { m_impl->flat_next(); }

Filesystem::Filesystem(
    std::shared_ptr<internal::abstract::ReadableFilesystem> impl)
    : m_impl{std::move(impl)} {
  if (m_impl == nullptr) {
    throw std::invalid_argument("impl must not be null");
  }
}

bool Filesystem::exists(const std::string &path) const {
  return m_impl->exists(internal::AbsPath(path));
}

bool Filesystem::is_file(const std::string &path) const {
  return m_impl->is_file(internal::AbsPath(path));
}

bool Filesystem::is_directory(const std::string &path) const {
  return m_impl->is_directory(internal::AbsPath(path));
}

FileWalker Filesystem::file_walker(const std::string &path) const {
  return FileWalker(m_impl->file_walker(internal::AbsPath(path)));
}

File Filesystem::open(const std::string &path) const {
  return File(m_impl->open(internal::AbsPath(path)));
}

} // namespace odr
