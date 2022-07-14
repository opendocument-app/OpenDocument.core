#include <odr/internal/common/file.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/resource.hpp>
#include <odr/internal/resource_data.hpp>

namespace odr::internal {

const Resources &Resources::instance() {
  static Resources instance;
  return instance;
}

Resources::Resources() {
  auto filesystem = std::make_shared<common::VirtualFilesystem>();

  for (std::uint32_t i = 0; i < resources::files_count; ++i) {
    Resource resource;
    resource.path = resources::files_path[i];
    resource.data = resources::files_data[i];
    resource.size = resources::files_size[i];
    m_resources.push_back(resource);

    filesystem->copy(std::make_shared<common::MemoryFile>(
                         std::string(resource.data, resource.size)),
                     resource.path);
  }

  m_filesystem = std::move(filesystem);
}

const std::vector<Resource> &Resources::resources() const {
  return m_resources;
}

std::shared_ptr<const abstract::Filesystem> Resources::filesystem() const {
  return m_filesystem;
}

} // namespace odr::internal
