#pragma once

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/iwork/iwork_types.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/// Test-only assembler for the layers an iWork package is made of: protobuf
/// fields, `TSP.ArchiveInfo` framing, a Snappy block, and the component list
/// that names the files. The engine has no writer and is not getting one —
/// this exists so a parser test can state its input inline rather than needing
/// a fixture for every shape, and must never grow into a writer API.
///
/// Field numbers are the ones `iwork_archive.cpp` reads, cited there to
/// `empty.pages Index/Metadata.iwa`.
namespace odr::test::iwork {

/// `TSP.PackageMetadata`, the object the component list lives in.
constexpr std::uint32_t package_metadata_type = 11006;
constexpr std::uint32_t package_metadata_components = 3;
constexpr std::uint32_t component_info_preferred_locator = 2;
constexpr std::uint32_t component_info_locator = 3;

inline std::string varint(std::uint64_t value) {
  std::string result;
  for (;;) {
    const auto byte = static_cast<char>(value & 0x7f);
    value >>= 7;
    result.push_back(value == 0 ? byte : static_cast<char>(byte | 0x80));
    if (value == 0) {
      return result;
    }
  }
}

inline std::string number_field(const std::uint32_t number,
                                const std::uint64_t value) {
  return varint(number << 3) + varint(value);
}

inline std::string message_field(const std::uint32_t number,
                                 const std::string &bytes) {
  return varint((number << 3) | 2) + varint(bytes.size()) + bytes;
}

/// `TSP.ArchiveInfo`: an identifier and one `MessageInfo` per payload message.
inline std::string archive_info(
    const std::uint64_t identifier,
    const std::vector<std::pair<std::uint32_t, std::size_t>> &messages) {
  std::string result = number_field(1, identifier);
  for (const auto &[type, length] : messages) {
    result += message_field(2, number_field(1, type) + number_field(3, length));
  }
  return result;
}

/// One archived object: its info, length-prefixed, then @p payload.
inline std::string
object(const std::uint64_t identifier,
       const std::vector<std::pair<std::uint32_t, std::size_t>> &messages,
       const std::string &payload) {
  const std::string info = archive_info(identifier, messages);
  return varint(info.size()) + info + payload;
}

/// One Snappy block holding @p data as a single literal — the compressor the
/// engine never needs, in the shape `snappy_decompress_block` reads.
inline std::string snappy_block(const std::string &data) {
  std::string result = varint(data.size());
  if (data.empty()) {
    return result;
  }

  // a literal tag carries its length inline up to 60 bytes, in the bytes
  // after it beyond that
  const std::size_t length = data.size() - 1;
  if (length < 60) {
    result.push_back(static_cast<char>(length << 2));
  } else {
    std::string bytes;
    for (std::size_t rest = length; rest != 0; rest >>= 8) {
      bytes.push_back(static_cast<char>(rest & 0xff));
    }
    result.push_back(static_cast<char>((59 + bytes.size()) << 2));
    result += bytes;
  }
  return result + data;
}

/// @p data as one `.iwa` file: a single block behind Apple's framing.
inline std::string iwa(const std::string &data) {
  const std::string block = snappy_block(data);
  const std::size_t length = block.size();
  const std::string header{'\0', static_cast<char>(length & 0xff),
                           static_cast<char>((length >> 8) & 0xff),
                           static_cast<char>((length >> 16) & 0xff)};
  return header + block;
}

/// A filesystem holding @p files as they are given.
inline std::shared_ptr<internal::abstract::ReadableFilesystem>
filesystem(const std::vector<std::pair<std::string, std::string>> &files) {
  auto result = std::make_shared<internal::VirtualFilesystem>();
  for (const auto &[path, data] : files) {
    result->copy(std::make_shared<internal::MemoryFile>(data),
                 internal::AbsPath(path));
  }
  return result;
}

/// The component list `Index/Metadata.iwa` carries, naming @p locators. A
/// component's name is its locator here — the fixtures are where the two
/// differ.
inline std::string package_metadata(const std::vector<std::string> &locators) {
  std::string list;
  for (const std::string &locator : locators) {
    list +=
        message_field(package_metadata_components,
                      message_field(component_info_preferred_locator, locator) +
                          message_field(component_info_locator, locator));
  }
  return object(2, {{package_metadata_type, list.size()}}, list);
}

/// A package: one `/Index/<locator>.iwa` per component of @p components, plus
/// the `Index/Metadata.iwa` naming them.
inline std::shared_ptr<internal::abstract::ReadableFilesystem>
package(const std::vector<std::pair<std::string, std::string>> &components) {
  std::vector<std::string> locators;
  std::vector<std::pair<std::string, std::string>> files;
  for (const auto &[locator, data] : components) {
    locators.push_back(locator);
    files.emplace_back("/Index/" + locator + ".iwa", iwa(data));
  }

  files.emplace_back("/Index/Metadata.iwa", iwa(package_metadata(locators)));
  return filesystem(files);
}

/// A `TP.DocumentArchive` whose body is the object @p body_identifier.
inline std::string document_archive(const std::uint64_t body_identifier) {
  return message_field(
      ::odr::internal::iwork::document_archive::body_storage,
      number_field(::odr::internal::iwork::reference::identifier,
                   body_identifier));
}

/// A `TSWP.StorageArchive` holding @p text, with a paragraph style run table
/// over the UTF-16 code unit indices @p paragraphs. `std::nullopt` writes no
/// table at all, which is not the same as an empty one.
inline std::string
text_storage(const std::string &text,
             const std::optional<std::vector<std::uint64_t>> &paragraphs) {
  namespace types = ::odr::internal::iwork;

  std::string result = message_field(types::text_storage::text, text);
  if (paragraphs.has_value()) {
    std::string table;
    for (const std::uint64_t index : *paragraphs) {
      table += message_field(
          types::attribute_table::entries,
          number_field(types::attribute_table_entry::character_index, index));
    }
    result += message_field(types::text_storage::paragraph_styles, table);
  }
  return result;
}

/// The object a synthetic package's body storage is filed under.
constexpr std::uint64_t body_identifier = 5;

/// The one-component package a `.pages` is: a root archive of @p root_type
/// whose body storage is @p storage.
inline std::shared_ptr<internal::abstract::ReadableFilesystem>
pages_package(const std::string &storage,
              const std::uint32_t root_type =
                  ::odr::internal::iwork::archive_type::pages_document) {
  const std::string root = document_archive(body_identifier);
  const std::string document =
      object(1, {{root_type, root.size()}}, root) +
      object(body_identifier,
             {{::odr::internal::iwork::archive_type::text_storage,
               storage.size()}},
             storage);
  return package({{"Document", document}});
}

} // namespace odr::test::iwork
