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

FileWalker &FileWalker::operator=(const odr::FileWalker &other) {
  m_impl = other.m_impl->clone();
  return *this;
}

FileWalker::operator bool() const { return m_impl.operator bool(); }

bool FileWalker::end() const { return m_impl ? m_impl->end() : true; }

std::uint32_t FileWalker::depth() const { return m_impl ? m_impl->depth() : 0; }

std::string FileWalker::path() const {
  return m_impl ? m_impl->path().string() : std::string("");
}

bool FileWalker::is_file() const { return m_impl ? m_impl->is_file() : false; }

bool FileWalker::is_directory() const {
  return m_impl ? m_impl->is_directory() : false;
}

void FileWalker::pop() {
  if (m_impl) {
    m_impl->pop();
  }
}

void FileWalker::next() {
  if (m_impl) {
    m_impl->next();
  }
}

void FileWalker::flat_next() {
  if (m_impl) {
    m_impl->flat_next();
  }
}

Filesystem::Filesystem(
    std::shared_ptr<internal::abstract::ReadableFilesystem> impl)
    : m_impl{std::move(impl)} {}

Filesystem::operator bool() const { return m_impl.operator bool(); }

bool Filesystem::exists(const std::string &path) const {
  return m_impl ? m_impl->exists(path) : false;
}

bool Filesystem::is_file(const std::string &path) const {
  return m_impl ? m_impl->is_file(path) : false;
}

bool Filesystem::is_directory(const std::string &path) const {
  return m_impl ? m_impl->is_directory(path) : false;
}

FileWalker Filesystem::file_walker(const std::string &path) const {
  return m_impl ? FileWalker(m_impl->file_walker(path)) : FileWalker();
}

File Filesystem::open(const std::string &path) const {
  return m_impl ? File(m_impl->open(path)) : File();
}

} // namespace odr
