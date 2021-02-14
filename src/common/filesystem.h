#ifndef ODR_COMMON_FILESYSTEM_H
#define ODR_COMMON_FILESYSTEM_H

#include <abstract/filesystem.h>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace odr::common {

class SystemFilesystem final : public abstract::Filesystem {
public:
  explicit SystemFilesystem(common::Path root);

  [[nodiscard]] bool exists(common::Path path) const final;
  [[nodiscard]] bool is_file(common::Path path) const final;
  [[nodiscard]] bool is_directory(common::Path path) const final;

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(common::Path path) const final;

  [[nodiscard]] std::shared_ptr<abstract::File>
  open(common::Path path) const final;

  std::unique_ptr<std::ostream> create_file(common::Path path) final;
  bool create_directory(common::Path path) final;

  bool remove(common::Path path) final;
  bool copy(common::Path from, common::Path to) final;
  std::shared_ptr<abstract::File> copy(abstract::File &from,
                                       common::Path to) final;
  std::shared_ptr<abstract::File> copy(std::shared_ptr<abstract::File> from,
                                       common::Path to) final;
  bool move(common::Path from, common::Path to) final;

private:
  common::Path m_root;

  [[nodiscard]] common::Path to_system_path(const common::Path &path) const;
};

class VirtualFilesystem final : public abstract::Filesystem {
public:
  VirtualFilesystem();

  [[nodiscard]] bool exists(common::Path path) const final;
  [[nodiscard]] bool is_file(common::Path path) const final;
  [[nodiscard]] bool is_directory(common::Path path) const final;

  [[nodiscard]] std::unique_ptr<abstract::FileWalker>
  file_walker(common::Path path) const final;

  [[nodiscard]] std::shared_ptr<abstract::File>
  open(common::Path path) const final;

  std::unique_ptr<std::ostream> create_file(common::Path path) final;
  bool create_directory(common::Path path) final;

  bool remove(common::Path path) final;
  bool copy(common::Path from, common::Path to) final;
  std::shared_ptr<abstract::File> copy(abstract::File &from,
                                       common::Path to) final;
  std::shared_ptr<abstract::File> copy(std::shared_ptr<abstract::File> from,
                                       common::Path to) final;
  bool move(common::Path from, common::Path to) final;

private:
  enum class NodeType {
    FILE,
    DIRECTORY,
  };
  struct Node {
    std::string name;
    NodeType type;

    Node(std::string name, NodeType type);
  };
  struct FileNode : Node {
    std::shared_ptr<abstract::File> file;

    FileNode(std::string name, std::shared_ptr<abstract::File> file);
  };
  struct DirectoryNode : Node {
    std::vector<std::variant<FileNode, DirectoryNode>> children;

    explicit DirectoryNode(std::string name);
  };

  DirectoryNode m_root;

  [[nodiscard]] Node *node_(const common::Path &path) const;
};

} // namespace odr::common

#endif // ODR_COMMON_FILESYSTEM_H
