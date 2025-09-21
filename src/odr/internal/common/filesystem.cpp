#include <odr/internal/common/filesystem.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/util/file_util.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <system_error>

namespace odr::internal {

namespace {
class SystemFileWalker final : public abstract::FileWalker {
public:
  SystemFileWalker(AbsPath root, const Path &path)
      : m_root{std::move(root)},
        m_iterator{std::filesystem::recursive_directory_iterator(path.path())} {
  }

  [[nodiscard]] std::unique_ptr<FileWalker> clone() const override {
    return std::make_unique<SystemFileWalker>(*this);
  }

  [[nodiscard]] bool equals(const FileWalker & /*rhs*/) const override {
    return false; // TODO
  }

  [[nodiscard]] bool end() const override {
    return m_iterator == std::filesystem::end(m_iterator);
  }

  [[nodiscard]] std::uint32_t depth() const override {
    return m_iterator.depth();
  }

  [[nodiscard]] AbsPath path() const override {
    return AbsPath("/").join(AbsPath(m_iterator->path()).rebase(m_root));
  }

  [[nodiscard]] bool is_file() const override {
    return m_iterator->is_regular_file();
  }

  [[nodiscard]] bool is_directory() const override {
    return m_iterator->is_directory();
  }

  void pop() override { return m_iterator.pop(); }

  void next() override { ++m_iterator; }

  void flat_next() override {
    m_iterator.disable_recursion_pending();
    ++m_iterator;
  }

private:
  AbsPath m_root;
  std::filesystem::recursive_directory_iterator m_iterator;
};
} // namespace

SystemFilesystem::SystemFilesystem(AbsPath root) : m_root{std::move(root)} {}

AbsPath SystemFilesystem::to_system_path_(const AbsPath &path) const {
  return m_root.join(path.rebase(AbsPath("/")));
}

bool SystemFilesystem::exists(const AbsPath &path) const {
  return std::filesystem::exists(to_system_path_(path).path());
}

bool SystemFilesystem::is_file(const AbsPath &path) const {
  return std::filesystem::is_regular_file(to_system_path_(path).path());
}

bool SystemFilesystem::is_directory(const AbsPath &path) const {
  return std::filesystem::is_directory(to_system_path_(path).path());
}

std::unique_ptr<abstract::FileWalker>
SystemFilesystem::file_walker(const AbsPath &path) const {
  return std::make_unique<SystemFileWalker>(m_root, to_system_path_(path));
}

std::shared_ptr<abstract::File>
SystemFilesystem::open(const AbsPath &path) const {
  return std::make_unique<DiskFile>(to_system_path_(path));
}

std::unique_ptr<std::ostream>
SystemFilesystem::create_file(const AbsPath &path) {
  return std::make_unique<std::ofstream>(
      util::file::create(to_system_path_(path).string()));
}

bool SystemFilesystem::create_directory(const AbsPath &path) {
  return std::filesystem::create_directory(to_system_path_(path).path());
}

bool SystemFilesystem::remove(const AbsPath &path) {
  return std::filesystem::remove(to_system_path_(path).path());
}

bool SystemFilesystem::copy(const AbsPath &from, const AbsPath &to) {
  std::error_code error_code;
  std::filesystem::copy(to_system_path_(from).path(),
                        to_system_path_(to).path(), error_code);
  if (error_code) {
    return false;
  }
  return true;
}

std::shared_ptr<abstract::File>
SystemFilesystem::copy(const abstract::File &from, const AbsPath &to) {
  const auto istream = from.stream();
  const auto ostream = create_file(to_system_path_(to));

  util::stream::pipe(*istream, *ostream);

  return open(to_system_path_(to));
}

std::shared_ptr<abstract::File>
SystemFilesystem::copy(const std::shared_ptr<abstract::File> from,
                       const AbsPath &to) {
  return copy(*from, to_system_path_(to));
}

bool SystemFilesystem::move(const AbsPath &from, const AbsPath &to) {
  std::error_code error_code;
  std::filesystem::rename(to_system_path_(from).path(),
                          to_system_path_(to).path(), error_code);
  if (error_code) {
    return false;
  }
  return true;
}

namespace {
class VirtualFileWalker final : public abstract::FileWalker {
public:
  using Files = std::map<AbsPath, std::shared_ptr<abstract::File>>;

  VirtualFileWalker(const AbsPath &root, const Files &files) {
    for (const auto &[path, file] : files) {
      if (path.ancestor_of(root)) {
        m_files[path] = file;
      }
    }

    m_iterator = std::begin(m_files);
  }

  [[nodiscard]] std::unique_ptr<FileWalker> clone() const override {
    return std::make_unique<VirtualFileWalker>(*this);
  }

  [[nodiscard]] bool equals(const FileWalker &rhs_) const override {
    auto &&rhs = dynamic_cast<const VirtualFileWalker &>(rhs_);
    return m_iterator == rhs.m_iterator;
  }

  [[nodiscard]] bool end() const override {
    return m_iterator == std::end(m_files);
  }

  [[nodiscard]] std::uint32_t depth() const override {
    return 0; // TODO
  }

  [[nodiscard]] AbsPath path() const override { return m_iterator->first; }

  [[nodiscard]] bool is_file() const override {
    return m_iterator->second.operator bool();
  }

  [[nodiscard]] bool is_directory() const override { return !is_file(); }

  void pop() override {
    // TODO
  }

  void next() override { ++m_iterator; }

  void flat_next() override {
    // TODO
  }

private:
  Files m_files;
  Files::iterator m_iterator;
};
} // namespace

bool VirtualFilesystem::exists(const AbsPath &path) const {
  return m_files.contains(path);
}

bool VirtualFilesystem::is_file(const AbsPath &path) const {
  const auto file_it = m_files.find(path);
  if (file_it == std::end(m_files)) {
    return false;
  }
  return static_cast<bool>(file_it->second);
}

bool VirtualFilesystem::is_directory(const AbsPath &path) const {
  const auto file_it = m_files.find(path);
  if (file_it == std::end(m_files)) {
    return false;
  }
  return !static_cast<bool>(file_it->second);
}

std::unique_ptr<abstract::FileWalker>
VirtualFilesystem::file_walker(const AbsPath &path) const {
  return std::make_unique<VirtualFileWalker>(path, m_files);
}

std::shared_ptr<abstract::File>
VirtualFilesystem::open(const AbsPath &path) const {
  const auto file_it = m_files.find(path);
  if (file_it == std::end(m_files)) {
    return {};
  }
  return file_it->second;
}

std::unique_ptr<std::ostream>
VirtualFilesystem::create_file(const AbsPath & /*path*/) {
  throw UnsupportedOperation();
}

bool VirtualFilesystem::create_directory(const AbsPath &path) {
  if (exists(path)) {
    return false;
  }
  m_files[path] = nullptr;
  return true;
}

bool VirtualFilesystem::remove(const AbsPath &path) {
  const auto file_it = m_files.find(path);
  if (file_it == std::end(m_files)) {
    return false;
  }
  m_files.erase(file_it);
  return true;
}

bool VirtualFilesystem::copy(const AbsPath &from, const AbsPath &to) {
  // TODO what about directories?

  const auto from_it = m_files.find(from);
  if (from_it == std::end(m_files)) {
    return false;
  }
  if (exists(to)) {
    return false;
  }
  m_files[to] = from_it->second;
  return true;
}

std::shared_ptr<abstract::File>
VirtualFilesystem::copy(const abstract::File & /*from*/,
                        const AbsPath & /*to*/) {
  throw UnsupportedOperation();
}

std::shared_ptr<abstract::File>
VirtualFilesystem::copy(std::shared_ptr<abstract::File> from,
                        const AbsPath &to) {
  if (exists(to)) {
    return {};
  }
  m_files[to] = from;
  return from;
}

bool VirtualFilesystem::move(const AbsPath &from, const AbsPath &to) {
  if (!copy(from, to)) {
    return false;
  }
  remove(from);
  return true;
}

} // namespace odr::internal
