#include <odr/filesystem.hpp>

#include <odr/file.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>

namespace odr {

FileWalker::FileWalker() = default;

FileWalker::FileWalker(std::unique_ptr<internal::abstract::FileWalker> impl)
    : m_impl{std::move(impl)} {}

FileWalker::FileWalker(const FileWalker &other)
    : m_impl{other.m_impl->clone()} {}

FileWalker::FileWalker(FileWalker &&other) noexcept = default;

FileWalker::~FileWalker() = default;

FileWalker &FileWalker::operator=(const odr::FileWalker &other) {
  if (this == &other) {
    return *this;
  }
  m_impl = other.m_impl->clone();
  return *this;
}

FileWalker &FileWalker::operator=(odr::FileWalker &&) noexcept = default;

FileWalker::operator bool() const { return m_impl != nullptr; }

bool FileWalker::end() const { return m_impl == nullptr || m_impl->end(); }

std::uint32_t FileWalker::depth() const {
  return m_impl != nullptr ? m_impl->depth() : 0;
}

std::string FileWalker::path() const {
  return m_impl != nullptr ? m_impl->path().string() : std::string("");
}

bool FileWalker::is_file() const {
  return m_impl != nullptr && m_impl->is_file();
}

bool FileWalker::is_directory() const {
  return m_impl != nullptr && m_impl->is_directory();
}

void FileWalker::pop() {
  if (m_impl != nullptr) {
    m_impl->pop();
  }
}

void FileWalker::next() {
  if (m_impl != nullptr) {
    m_impl->next();
  }
}

void FileWalker::flat_next() {
  if (m_impl != nullptr) {
    m_impl->flat_next();
  }
}

Filesystem::Filesystem(
    std::shared_ptr<internal::abstract::ReadableFilesystem> impl)
    : m_impl{std::move(impl)} {}

Filesystem::operator bool() const { return m_impl != nullptr; }

bool Filesystem::exists(const std::string &path) const {
  return m_impl != nullptr && m_impl->exists(internal::AbsPath(path));
}

bool Filesystem::is_file(const std::string &path) const {
  return m_impl != nullptr && m_impl->is_file(internal::AbsPath(path));
}

bool Filesystem::is_directory(const std::string &path) const {
  return m_impl != nullptr && m_impl->is_directory(internal::AbsPath(path));
}

FileWalker Filesystem::file_walker(const std::string &path) const {
  return m_impl != nullptr
             ? FileWalker(m_impl->file_walker(internal::AbsPath(path)))
             : FileWalker();
}

File Filesystem::open(const std::string &path) const {
  return m_impl != nullptr ? File(m_impl->open(internal::AbsPath(path)))
                           : File();
}

} // namespace odr
