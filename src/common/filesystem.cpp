#include <common/file.h>
#include <common/filesystem.h>
#include <common/path.h>
#include <filesystem>
#include <fstream>
#include <odr/exceptions.h>
#include <system_error>
#include <util/stream_util.h>

namespace odr::common {

namespace {
class SystemFileWalker final : public abstract::FileWalker {
public:
  SystemFileWalker(Path root, const Path &path)
      : m_root{std::move(root)},
        m_iterator{std::filesystem::recursive_directory_iterator(path)} {}

  [[nodiscard]] std::unique_ptr<FileWalker> clone() const final {
    return std::make_unique<SystemFileWalker>(*this);
  }

  [[nodiscard]] bool equals(const FileWalker &rhs) const final {
    return false; // TODO
  }

  [[nodiscard]] bool end() const final {
    return m_iterator == std::filesystem::end(m_iterator);
  }

  [[nodiscard]] std::uint32_t depth() const final { return m_iterator.depth(); }

  [[nodiscard]] Path path() const final {
    return Path("/").join(Path(m_iterator->path()).rebase(m_root));
  }

  [[nodiscard]] bool is_file() const final {
    return m_iterator->is_regular_file();
  }

  [[nodiscard]] bool is_directory() const final {
    return m_iterator->is_directory();
  }

  void pop() final { return m_iterator.pop(); }

  void next() final { ++m_iterator; }

  void flat_next() final {
    m_iterator.disable_recursion_pending();
    ++m_iterator;
  }

private:
  Path m_root;
  std::filesystem::recursive_directory_iterator m_iterator;
};
} // namespace

SystemFilesystem::SystemFilesystem(Path root) : m_root{std::move(root)} {}

Path SystemFilesystem::to_system_path_(const Path &path) const {
  return m_root.join(path.rebase("/"));
}

bool SystemFilesystem::exists(Path path) const {
  return std::filesystem::exists(to_system_path_(path));
}

bool SystemFilesystem::is_file(Path path) const {
  return std::filesystem::is_regular_file(to_system_path_(path));
}

bool SystemFilesystem::is_directory(Path path) const {
  return std::filesystem::is_directory(to_system_path_(path));
}

std::unique_ptr<abstract::FileWalker>
SystemFilesystem::file_walker(Path path) const {
  return std::make_unique<SystemFileWalker>(m_root, to_system_path_(path));
}

std::shared_ptr<abstract::File> SystemFilesystem::open(Path path) const {
  return std::make_unique<DiscFile>(to_system_path_(path));
}

std::unique_ptr<std::ostream> SystemFilesystem::create_file(Path path) {
  return std::make_unique<std::ofstream>(to_system_path_(path));
}

bool SystemFilesystem::create_directory(Path path) {
  return std::filesystem::create_directory(to_system_path_(path));
}

bool SystemFilesystem::remove(Path path) {
  return std::filesystem::remove(to_system_path_(path));
}

bool SystemFilesystem::copy(Path from, Path to) {
  std::error_code error_code;
  std::filesystem::copy(to_system_path_(from), to_system_path_(to), error_code);
  if (error_code) {
    return false;
  }
  return true;
}

std::shared_ptr<abstract::File>
SystemFilesystem::copy(const abstract::File &from, Path to) {
  auto istream = from.read();
  auto ostream = create_file(to_system_path_(to));

  util::stream::pipe(*istream, *ostream);

  return open(to_system_path_(to));
}

std::shared_ptr<abstract::File>
SystemFilesystem::copy(std::shared_ptr<abstract::File> from, Path to) {
  return copy(*from, to_system_path_(to));
}

bool SystemFilesystem::move(Path from, Path to) {
  std::error_code error_code;
  std::filesystem::rename(to_system_path_(from), to_system_path_(to),
                          error_code);
  if (error_code) {
    return false;
  }
  return true;
}

namespace {
class VirtualFileWalker final : public abstract::FileWalker {
public:
  VirtualFileWalker(
      const Path &root,
      const std::map<Path, std::shared_ptr<abstract::File>> &files) {
    for (auto &&f : files) {
      if (f.first.ancestor_of(root)) {
        m_files[f.first] = f.second;
      }
    }

    m_iterator = std::begin(m_files);
  }

  [[nodiscard]] std::unique_ptr<FileWalker> clone() const final {
    return std::make_unique<VirtualFileWalker>(*this); // TODO
  }

  [[nodiscard]] bool equals(const FileWalker &rhs) const final {
    return false; // TODO
  }

  [[nodiscard]] bool end() const final {
    return m_iterator == std::end(m_files);
  }

  [[nodiscard]] std::uint32_t depth() const final {
    return 0; // TODO
  }

  [[nodiscard]] Path path() const final {
    return {}; // TODO
  }

  [[nodiscard]] bool is_file() const final {
    return false; // TODO
  }

  [[nodiscard]] bool is_directory() const final {
    return false; // TODO
  }

  void pop() final {
    // TODO
  }

  void next() final { ++m_iterator; }

  void flat_next() final {
    // TODO
  }

private:
  using Files = std::map<Path, std::shared_ptr<abstract::File>>;

  Files m_files;
  Files::iterator m_iterator;
};
} // namespace

bool VirtualFilesystem::exists(Path path) const {
  return m_files.find(path) != std::end(m_files);
}

bool VirtualFilesystem::is_file(Path path) const {
  auto file_it = m_files.find(path);
  if (file_it == std::end(m_files)) {
    return false;
  }
  return (bool)file_it->second;
}

bool VirtualFilesystem::is_directory(Path path) const {
  auto file_it = m_files.find(path);
  if (file_it == std::end(m_files)) {
    return false;
  }
  return !(bool)file_it->second;
}

std::unique_ptr<abstract::FileWalker>
VirtualFilesystem::file_walker(Path path) const {
  throw UnsupportedOperation();
  // TODO
  // return std::make_unique<VirtualFileWalker>(std::move(path), m_files);
}

std::shared_ptr<abstract::File> VirtualFilesystem::open(Path path) const {
  auto file_it = m_files.find(path);
  if (file_it == std::end(m_files)) {
    return {};
  }
  return file_it->second;
}

std::unique_ptr<std::ostream> VirtualFilesystem::create_file(Path path) {
  throw UnsupportedOperation();
}

bool VirtualFilesystem::create_directory(Path path) {
  if (exists(path)) {
    return false;
  }
  m_files[path] = nullptr;
  return true;
}

bool VirtualFilesystem::remove(Path path) {
  auto file_it = m_files.find(path);
  if (file_it == std::end(m_files)) {
    return false;
  }
  m_files.erase(file_it);
  return true;
}

bool VirtualFilesystem::copy(Path from, Path to) {
  // TODO what about directories?

  auto from_it = m_files.find(from);
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
VirtualFilesystem::copy(const abstract::File &from, Path to) {
  throw UnsupportedOperation();
}

std::shared_ptr<abstract::File>
VirtualFilesystem::copy(std::shared_ptr<abstract::File> from, Path to) {
  if (exists(to)) {
    return {};
  }
  m_files[to] = from;
  return from;
}

bool VirtualFilesystem::move(Path from, Path to) {
  if (!copy(from, to)) {
    return false;
  }
  remove(from);
  return true;
}

} // namespace odr::common
