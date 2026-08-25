#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace odr::internal {
class AbsPath;
} // namespace odr::internal

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::iwork {

/// One object of an `.iwa`, per `TSP.ArchiveInfo`. Where it holds more than
/// one message, only the first is modelled.
struct Object final {
  std::uint64_t identifier{};
  std::uint32_t type{};
  std::string_view payload;
};

/// The objects of one `.iwa`, over the bytes it decompressed to; payloads are
/// views into those bytes.
class Component final {
public:
  Component(std::string locator, std::string data);

  /// The file it was loaded from, without `/Index/` and `.iwa`. Unlike its
  /// name, this is unique across the package.
  [[nodiscard]] const std::string &locator() const noexcept;
  [[nodiscard]] const std::vector<Object> &objects() const noexcept;

private:
  std::string m_locator;
  std::unique_ptr<const std::string> m_data;
  std::vector<Object> m_objects;
};

/// An iWork package. Objects reference each other across components, so the
/// list from `Index/Metadata.iwa` is read first and a component decompressed
/// when something in it is asked for.
class Package final {
public:
  explicit Package(const abstract::ReadableFilesystem &filesystem);

  /// Whether the package's component list holds one named @p name. Reads the
  /// list only — nothing is decompressed.
  [[nodiscard]] bool has_component(std::string_view name) const noexcept;

  /// The first component named @p name — a name is not unique. Throws when
  /// the package holds none.
  const Component &component(std::string_view name);

  /// The object @p identifier names, loading components until it turns up.
  const Object &object(std::uint64_t identifier);

private:
  /// One entry of the component list: the name a component is known by, and
  /// the file it lives in.
  struct ComponentInfo final {
    std::string name;
    std::string locator;
  };

  const abstract::ReadableFilesystem *m_filesystem{nullptr};
  std::vector<ComponentInfo> m_component_infos;
  std::deque<Component> m_components;
  std::unordered_map<std::uint64_t, const Object *> m_objects;

  /// The first component list entry named @p name, or `end()`.
  [[nodiscard]] std::vector<ComponentInfo>::const_iterator
  find_(std::string_view name) const noexcept;

  const Component &load_(const ComponentInfo &info);
};

/// Reads @p path and undoes its `.iwa` framing.
std::string read_iwa(const abstract::ReadableFilesystem &filesystem,
                     const AbsPath &path);

/// Splits a decompressed `.iwa` into its objects, viewing @p data.
std::vector<Object> read_objects(std::string_view data);

} // namespace odr::internal::iwork
