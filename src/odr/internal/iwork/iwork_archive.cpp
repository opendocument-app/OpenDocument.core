#include <odr/internal/iwork/iwork_archive.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/iwork/iwork_protobuf.hpp>
#include <odr/internal/iwork/iwork_snappy.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <algorithm>
#include <istream>
#include <stdexcept>
#include <utility>

namespace odr::internal {

namespace {

/// Field numbers of `TSP.PackageMetadata` and the `ComponentInfo` it repeats,
/// read off `empty.pages Index/Metadata.iwa` (object 2, type 11006).
constexpr std::uint32_t package_metadata_components = 3;
constexpr std::uint32_t component_info_identifier = 1;
constexpr std::uint32_t component_info_preferred_locator = 2;
constexpr std::uint32_t component_info_locator = 3;

/// Field numbers of `TSP.ArchiveInfo` and the `MessageInfo` it repeats.
constexpr std::uint32_t archive_info_identifier = 1;
constexpr std::uint32_t archive_info_messages = 2;
constexpr std::uint32_t message_info_type = 1;
constexpr std::uint32_t message_info_length = 3;

AbsPath component_path(const std::string &locator) {
  return AbsPath("/Index").join(RelPath(locator + ".iwa"));
}

} // namespace

std::string iwork::read_iwa(const abstract::ReadableFilesystem &filesystem,
                            const AbsPath &path) {
  const std::shared_ptr<abstract::File> file = filesystem.open(path);
  if (!file) {
    throw std::runtime_error("iwork: missing " + path.string());
  }
  const std::unique_ptr<std::istream> stream = file->stream();
  return iwa_decompress(util::stream::read(*stream));
}

std::vector<iwork::Object> iwork::read_objects(const std::string_view data) {
  std::vector<Object> result;

  std::size_t position = 0;
  while (position < data.size()) {
    const std::uint64_t info_length = read_varint(data, position);
    if (info_length > data.size() - position) {
      throw std::runtime_error("iwork: archive info runs past the component");
    }
    const Message info(data.substr(position, info_length));
    position += info_length;

    Object object;
    object.identifier = info.number_field(archive_info_identifier).value_or(0);

    // the payload holds every message the info names, back to back; only the
    // first is modelled, the length of the rest is what skips them
    std::size_t payload_length = 0;
    std::size_t first_length = 0;
    bool first = true;
    for (const Field &message : info.repeated_field(archive_info_messages)) {
      if (message.type != WireType::length_delimited) {
        throw std::runtime_error("iwork: malformed message info");
      }
      const Message message_info(message.bytes);
      const std::uint64_t length =
          message_info.number_field(message_info_length).value_or(0);
      if (first) {
        object.type = static_cast<std::uint32_t>(
            message_info.number_field(message_info_type).value_or(0));
        first_length = length;
        first = false;
      }
      payload_length += length;
    }

    if (payload_length > data.size() - position) {
      throw std::runtime_error("iwork: object payload runs past the component");
    }
    object.payload = data.substr(position, first_length);
    position += payload_length;

    result.push_back(object);
  }

  return result;
}

iwork::Component::Component(std::string locator, std::string data)
    : m_locator{std::move(locator)},
      m_data{std::make_unique<const std::string>(std::move(data))},
      m_objects{read_objects(*m_data)} {}

const std::string &iwork::Component::locator() const noexcept {
  return m_locator;
}

const std::vector<iwork::Object> &iwork::Component::objects() const noexcept {
  return m_objects;
}

iwork::Package::Package(const abstract::ReadableFilesystem &filesystem)
    : m_filesystem{&filesystem} {
  const std::string data = read_iwa(filesystem, AbsPath("/Index/Metadata.iwa"));
  const std::vector<Object> objects = read_objects(data);
  if (objects.empty()) {
    throw std::runtime_error("iwork: empty package metadata");
  }

  const Message metadata(objects.front().payload);
  for (const Field &component :
       metadata.repeated_field(package_metadata_components)) {
    if (component.type != WireType::length_delimited) {
      throw std::runtime_error("iwork: malformed component info");
    }
    const Message info(component.bytes);

    ComponentInfo result;
    result.identifier =
        info.number_field(component_info_identifier).value_or(0);
    result.name = std::string(info.bytes_field(component_info_preferred_locator)
                                  .value_or(std::string_view()));
    result.locator = std::string(
        info.bytes_field(component_info_locator).value_or(result.name));
    if (result.name.empty()) {
      throw std::runtime_error("iwork: component without a name");
    }
    m_component_infos.push_back(std::move(result));
  }
}

const iwork::Component &iwork::Package::component(const std::string &name) {
  const auto it =
      std::ranges::find(m_component_infos, name, &ComponentInfo::name);
  if (it == std::ranges::end(m_component_infos)) {
    throw std::runtime_error("iwork: no component named " + name);
  }
  return load_(*it);
}

const iwork::Object &iwork::Package::object(const std::uint64_t identifier) {
  if (const auto it = m_objects.find(identifier); it != m_objects.end()) {
    return *it->second;
  }

  for (const ComponentInfo &info : m_component_infos) {
    load_(info);
    if (const auto it = m_objects.find(identifier); it != m_objects.end()) {
      return *it->second;
    }
  }

  throw std::runtime_error("iwork: no object " + std::to_string(identifier));
}

const iwork::Component &iwork::Package::load_(const ComponentInfo &info) {
  // by locator, not by name: a name is shared across components, so keying on
  // it would hand back the wrong file and leave the other never loaded
  if (const auto it =
          std::ranges::find(m_components, info.locator, &Component::locator);
      it != std::ranges::end(m_components)) {
    return *it;
  }

  const Component &component = m_components.emplace_back(
      info.locator, read_iwa(*m_filesystem, component_path(info.locator)));
  for (const Object &object : component.objects()) {
    m_objects.emplace(object.identifier, &object);
  }
  return component;
}

} // namespace odr::internal
