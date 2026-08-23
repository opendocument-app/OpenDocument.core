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

/// One archived object. `TSP.ArchiveInfo` names its identifier (field 1) and,
/// per payload message, a `MessageInfo` (field 2) carrying the message type
/// and its length. An object usually holds one message; where it holds more,
/// only the first is modelled.
///
/// Verified on `empty.pages Index/Document.iwa +0`: `08 01` (identifier 1),
/// `12 52` (an 82-byte `MessageInfo`), `08 90 4e` (type 10000), `18 e0 0c`
/// (payload length 1632).
struct Object final {
  std::uint64_t identifier{};
  std::uint32_t type{};
  std::string_view payload;
};

/// The objects of one `.iwa`, over the bytes it decompressed to. Object
/// payloads are views into those bytes.
class Component final {
public:
  Component(std::string locator, std::string data);

  /// The file the component was loaded from, without `/Index/` and `.iwa`.
  /// Unlike its name, this is unique across the package.
  [[nodiscard]] const std::string &locator() const noexcept;
  [[nodiscard]] const std::vector<Object> &objects() const noexcept;

private:
  std::string m_locator;
  std::unique_ptr<const std::string> m_data;
  std::vector<Object> m_objects;
};

/// An iWork package: the component list from `Index/Metadata.iwa`, and the
/// components loaded from it so far.
///
/// Objects reference each other by identifier across components, so the list
/// is read first and a component is decompressed when something in it is
/// asked for.
class Package final {
public:
  explicit Package(const abstract::ReadableFilesystem &filesystem);

  /// The first component named @p name in the package's component list — a
  /// name is not unique, `Tables/DataList` names dozens. Throws when the
  /// package holds none.
  const Component &component(const std::string &name);

  /// The object @p identifier names, loading components until it is found.
  /// Throws when no component holds it.
  const Object &object(std::uint64_t identifier);

private:
  /// One entry of `TSP.PackageMetadata`'s component list: the identifier of
  /// the component's root object, the name it is known by, and the file it
  /// lives in — which carries an identifier suffix often enough that the file
  /// name is not a way to find it.
  struct ComponentInfo final {
    std::uint64_t identifier{};
    std::string name;
    std::string locator;
  };

  const abstract::ReadableFilesystem *m_filesystem{nullptr};
  std::vector<ComponentInfo> m_component_infos;
  std::deque<Component> m_components;
  std::unordered_map<std::uint64_t, const Object *> m_objects;

  const Component &load_(const ComponentInfo &info);
};

/// Reads @p path off @p filesystem and undoes its `.iwa` framing.
std::string read_iwa(const abstract::ReadableFilesystem &filesystem,
                     const AbsPath &path);

/// Splits a decompressed `.iwa` into its objects, over @p data.
std::vector<Object> read_objects(std::string_view data);

} // namespace odr::internal::iwork
