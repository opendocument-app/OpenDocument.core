#include <common/file.h>
#include <common/filesystem.h>
#include <common/path.h>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <util/stream_util.h>

namespace odr::common {

namespace {
class SystemFileWalker final : public abstract::FileWalker {
public:
  explicit SystemFileWalker(common::Path root, const common::Path &path)
      : m_root{std::move(root)},
        m_iterator{std::filesystem::recursive_directory_iterator(path)} {}

  [[nodiscard]] std::unique_ptr<FileWalker> clone() const final {
    return std::make_unique<SystemFileWalker>(*this);
  }

  [[nodiscard]] bool equals(const FileWalker &rhs) const final {
    return false; // TODO
  }

  [[nodiscard]] std::uint32_t depth() const final { return m_iterator.depth(); }

  [[nodiscard]] common::Path path() const final {
    return common::Path("/").join(
        common::Path(m_iterator->path()).rebase(m_root));
  }

  [[nodiscard]] bool is_file() const final {
    return m_iterator->is_regular_file();
  }

  [[nodiscard]] bool is_directory() const final {
    return m_iterator->is_directory();
  }

  [[nodiscard]] bool has_next() const final {
    auto tmp = m_iterator;
    ++tmp;
    return tmp != std::filesystem::end(m_iterator);
  }

  [[nodiscard]] bool has_flat_next() const final {
    auto tmp = m_iterator;
    tmp.disable_recursion_pending();
    ++tmp;
    return tmp != std::filesystem::end(m_iterator);
  }

  void pop() final { return m_iterator.pop(); }

  void next() final { ++m_iterator; }

  void flat_next() final {
    m_iterator.disable_recursion_pending();
    ++m_iterator;
  }

private:
  common::Path m_root;
  std::filesystem::recursive_directory_iterator m_iterator;
};
} // namespace

SystemFilesystem::SystemFilesystem(common::Path root)
    : m_root{std::move(root)} {}

common::Path SystemFilesystem::to_system_path_(const common::Path &path) const {
  return m_root.join(path.rebase("/"));
}

bool SystemFilesystem::exists(common::Path path) const {
  return std::filesystem::exists(to_system_path_(path));
}

bool SystemFilesystem::is_file(common::Path path) const {
  return std::filesystem::is_regular_file(to_system_path_(path));
}

bool SystemFilesystem::is_directory(common::Path path) const {
  return std::filesystem::is_directory(to_system_path_(path));
}

std::unique_ptr<abstract::FileWalker>
SystemFilesystem::file_walker(common::Path path) const {
  return std::make_unique<SystemFileWalker>(m_root, to_system_path_(path));
}

std::shared_ptr<abstract::File>
SystemFilesystem::open(common::Path path) const {
  return std::make_unique<common::DiscFile>(to_system_path_(path));
}

std::unique_ptr<std::ostream> SystemFilesystem::create_file(common::Path path) {
  return std::make_unique<std::ofstream>(to_system_path_(path));
}

bool SystemFilesystem::create_directory(common::Path path) {
  return std::filesystem::create_directory(to_system_path_(path));
}

bool SystemFilesystem::remove(common::Path path) {
  return std::filesystem::remove(to_system_path_(path));
}

bool SystemFilesystem::copy(common::Path from, common::Path to) {
  std::error_code error_code;
  std::filesystem::copy(to_system_path_(from), to_system_path_(to), error_code);
  if (error_code) {
    return false;
  }
  return true;
}

std::shared_ptr<abstract::File> SystemFilesystem::copy(abstract::File &from,
                                                       common::Path to) {
  auto istream = from.read();
  auto ostream = create_file(to_system_path_(to));

  util::stream::pipe(*istream, *ostream);

  return open(to_system_path_(to));
}

std::shared_ptr<abstract::File>
SystemFilesystem::copy(std::shared_ptr<abstract::File> from, common::Path to) {
  return copy(*from, to_system_path_(to));
}

bool SystemFilesystem::move(common::Path from, common::Path to) {
  std::error_code error_code;
  std::filesystem::rename(to_system_path_(from), to_system_path_(to), error_code);
  if (error_code) {
    return false;
  }
  return true;
}

VirtualFilesystem::Node::Node(std::string name, const NodeType type)
    : name{std::move(name)}, type{type} {}

VirtualFilesystem::FileNode::FileNode(std::string name,
                                      std::shared_ptr<abstract::File> file)
    : Node(std::move(name), NodeType::FILE), file{std::move(file)} {}

VirtualFilesystem::DirectoryNode::DirectoryNode(std::string name)
    : Node(std::move(name), NodeType::DIRECTORY) {}

VirtualFilesystem::VirtualFilesystem() : m_root{""} {}

VirtualFilesystem::Node *VirtualFilesystem::node_(const common::Path &path) const {
  return {}; // TODO
}

bool VirtualFilesystem::exists(common::Path path) const {
  return node_(path) != nullptr;
}

bool VirtualFilesystem::is_file(common::Path path) const {
  auto node = node_(path);
  if (node == nullptr) {
    return false;
  }
  return node->type == NodeType::FILE;
}

bool VirtualFilesystem::is_directory(common::Path path) const {
  auto node = node_(path);
  if (node == nullptr) {
    return false;
  }
  return node->type == NodeType::DIRECTORY;
}

std::unique_ptr<abstract::FileWalker>
VirtualFilesystem::file_walker(common::Path path) const {
  return {}; // TODO
}

std::shared_ptr<abstract::File>
VirtualFilesystem::open(common::Path path) const {
  auto node = node_(path);
  if (node == nullptr) {
    return {};
  }
  return reinterpret_cast<FileNode *>(node)->file;
}

std::unique_ptr<std::ostream>
VirtualFilesystem::create_file(common::Path path) {
  return {}; // TODO
}

bool VirtualFilesystem::create_directory(common::Path path) {
  return {}; // TODO
}

bool VirtualFilesystem::remove(common::Path path) {
  return {}; // TODO
}

bool VirtualFilesystem::copy(common::Path from, common::Path to) {
  return {}; // TODO
}

std::shared_ptr<abstract::File> VirtualFilesystem::copy(abstract::File &from,
                                                        common::Path to) {
  return {}; // TODO
}

std::shared_ptr<abstract::File>
VirtualFilesystem::copy(std::shared_ptr<abstract::File> from, common::Path to) {
  return {}; // TODO
}

bool VirtualFilesystem::move(common::Path from, common::Path to) {
  return {}; // TODO
}

} // namespace odr::common
