#include <odr/internal/resource.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/resource_data.hpp>

#include <odr/file.hpp>
#include <odr/filesystem.hpp>

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
                     common::Path(resource.path));
  }

  m_filesystem = std::move(filesystem);
}

const std::vector<Resource> &Resources::resources() {
  return instance().m_resources;
}

Filesystem Resources::filesystem() {
  return Filesystem(std::dynamic_pointer_cast<abstract::ReadableFilesystem>(
      instance().m_filesystem));
}

File Resources::open(const common::Path &path) {
  return filesystem().open(path);
}

} // namespace odr::internal
