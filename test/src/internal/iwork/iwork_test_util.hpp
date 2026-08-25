#pragma once

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/iwork/iwork_types.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/// Test-only assembler for the layers an iWork package is made of, so a test
/// can state its input inline instead of needing a fixture. Field numbers are
/// the ones `iwork_archive.cpp` reads. Must never grow into a writer API.
namespace odr::test::iwork {

/// The engine's field numbers and archive types, which this namespace shadows.
namespace types = internal::iwork;

/// The object the component list lives in.
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

/// A `fixed32` field, which is how iWork writes the `float`s of a geometry.
inline std::string float_field(const std::uint32_t number, const float value) {
  const auto bits = std::bit_cast<std::uint32_t>(value);
  std::string result = varint((number << 3) | 5);
  for (std::uint32_t shift = 0; shift < 32; shift += 8) {
    result.push_back(static_cast<char>((bits >> shift) & 0xff));
  }
  return result;
}

/// A `TSP.Reference` to @p identifier, in field @p number.
inline std::string reference_field(const std::uint32_t number,
                                   const std::uint64_t identifier) {
  return message_field(number,
                       number_field(types::reference::identifier, identifier));
}

/// A `TSP.Point`, which a size is written as too.
inline std::string point(const float x, const float y) {
  return float_field(types::point::x, x) + float_field(types::point::y, y);
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

/// One Snappy block holding @p data as a single literal.
inline std::string snappy_block(const std::string &data) {
  std::string result = varint(data.size());
  if (data.empty()) {
    return result;
  }

  // a literal tag carries its length inline up to 60 bytes
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

/// The component list `Index/Metadata.iwa` carries. A component's name is its
/// locator here; the fixtures are where the two differ.
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

/// One `/Index/<locator>.iwa` per component, plus the metadata naming them.
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
  return reference_field(types::document_archive::body_storage,
                         body_identifier);
}

/// A `TSWP.StorageArchive`: @p text and a paragraph style run table over the
/// UTF-16 code unit indices @p paragraphs. `std::nullopt` writes no table,
/// which is not an empty one.
inline std::string
text_storage(const std::string &text,
             const std::optional<std::vector<std::uint64_t>> &paragraphs) {
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

/// A `TSWP.ShapeArchive`: a drawable at @p rect holding the text storage
/// @p storage_identifier.
inline std::string text_shape(const std::uint64_t storage_identifier,
                              const float x, const float y, const float width,
                              const float height) {
  const std::string geometry =
      message_field(types::geometry_archive::position, point(x, y)) +
      message_field(types::geometry_archive::size, point(width, height));
  const std::string drawable =
      message_field(types::drawable_archive::geometry, geometry);
  const std::string shape =
      message_field(types::shape_archive::drawable, drawable);
  return message_field(types::text_shape::shape, shape) +
         reference_field(types::text_shape::storage, storage_identifier);
}

/// One slide of a synthetic deck: the text of each of its boxes, and where the
/// box sits.
struct SlideBox final {
  std::string text;
  std::optional<std::vector<std::uint64_t>> paragraphs{};
  float x{};
  float y{};
  float width{};
  float height{};
  /// Wrapped in a `KN.PlaceholderArchive` rather than standing on its own.
  bool placeholder{};
  /// How many times the slide's drawable list names this box. A deck Keynote
  /// wrote names each of its drawables once; a hand-built one may repeat a
  /// reference to make the parse expand what a few bytes name.
  std::size_t repeats{1};
};

/// The size of a synthetic deck's slides, in points.
struct SlideSize final {
  float width{1024};
  float height{768};
};

/// A `.key` package: a root archive holding a show, whose slide tree names one
/// `KN.SlideNodeArchive` per slide. `std::nullopt` writes a show with no size
/// at all, which is not one of zero size. Carries a `Slide` component, which
/// is what tells a Keynote package from a Numbers one.
inline std::shared_ptr<internal::abstract::ReadableFilesystem>
keynote_package(const std::vector<std::vector<SlideBox>> &slides,
                const std::optional<SlideSize> &slide_size = SlideSize{}) {
  constexpr std::uint64_t show_identifier = 2;
  // identifiers are handed out in blocks so a slide's objects never collide
  constexpr std::uint64_t slide_block = 100;

  std::string tree;
  std::string objects;
  std::uint64_t identifier = slide_block;

  for (const std::vector<SlideBox> &boxes : slides) {
    const std::uint64_t node_identifier = identifier;
    const std::uint64_t slide_identifier = identifier + 1;
    identifier += slide_block;

    tree += reference_field(types::slide_tree::nodes, node_identifier);

    const std::string node =
        reference_field(types::slide_node::slide, slide_identifier);
    objects +=
        object(node_identifier,
               {{types::archive_type::keynote_slide_node, node.size()}}, node);

    std::string slide;
    std::string drawables;
    std::uint64_t box_identifier = slide_identifier + 1;
    for (const SlideBox &box : boxes) {
      const std::uint64_t drawable_identifier = box_identifier++;
      const std::uint64_t storage_identifier = box_identifier++;

      for (std::size_t repeat = 0; repeat < box.repeats; ++repeat) {
        slide += reference_field(types::slide_archive::drawables,
                                 drawable_identifier);
      }

      const std::string shape =
          text_shape(storage_identifier, box.x, box.y, box.width, box.height);
      const std::string payload =
          box.placeholder
              ? message_field(types::placeholder_archive::shape, shape)
              : shape;
      const std::uint32_t type = box.placeholder
                                     ? types::archive_type::keynote_placeholder
                                     : types::archive_type::text_shape;
      drawables +=
          object(drawable_identifier, {{type, payload.size()}}, payload);

      const std::string storage = text_storage(box.text, box.paragraphs);
      drawables += object(storage_identifier,
                          {{types::archive_type::text_storage, storage.size()}},
                          storage);
    }

    objects +=
        object(slide_identifier,
               {{types::archive_type::keynote_slide, slide.size()}}, slide) +
        drawables;
  }

  std::string show = message_field(types::show_archive::slide_tree, tree);
  if (slide_size.has_value()) {
    show += message_field(types::show_archive::size,
                          point(slide_size->width, slide_size->height));
  }
  const std::string root =
      reference_field(types::document_archive::show, show_identifier);

  const std::string document =
      object(1, {{types::archive_type::app_document, root.size()}}, root) +
      object(show_identifier,
             {{types::archive_type::keynote_show, show.size()}}, show) +
      objects;

  return package({{"Document", document}, {"Slide", std::string()}});
}

/// The identifier a synthetic body storage is filed under.
constexpr std::uint64_t body_identifier = 5;

/// A one-component package: a root archive of @p root_type whose body is
/// @p storage.
inline std::shared_ptr<internal::abstract::ReadableFilesystem> pages_package(
    const std::string &storage,
    const std::uint32_t root_type = types::archive_type::pages_document) {
  const std::string root = document_archive(body_identifier);
  const std::string document =
      object(1, {{root_type, root.size()}}, root) +
      object(body_identifier,
             {{types::archive_type::text_storage, storage.size()}}, storage);
  return package({{"Document", document}});
}

/// One packed cell record: the twelve-byte header, then the value the flags
/// name. @p version and @p type are taken as given so a test can state a
/// record the reader has not mapped.
inline std::string
cell_record(const std::uint8_t type, const std::uint32_t flags,
            const std::string &value,
            const std::uint8_t version = types::cell::version) {
  std::string result(types::cell::header_size, '\0');
  result[types::cell::version_offset] = static_cast<char>(version);
  result[types::cell::type_offset] = static_cast<char>(type);
  for (std::size_t i = 0; i < 4; ++i) {
    result[types::cell::flags_offset + i] =
        static_cast<char>((flags >> (i * 8)) & 0xff);
  }
  return result + value;
}

/// A cell holding a `double` in the field @p flag names.
inline std::string number_cell(const std::uint8_t type,
                               const std::uint32_t flag, const double value) {
  const auto bits = std::bit_cast<std::uint64_t>(value);
  std::string bytes;
  for (std::uint32_t shift = 0; shift < 64; shift += 8) {
    bytes.push_back(static_cast<char>((bits >> shift) & 0xff));
  }
  return cell_record(type, flag, bytes);
}

/// One row of a tile: @p cells packed back to back, and the offsets table that
/// says where each column's record starts. An empty entry writes `-1`, the
/// column the row holds no cell for.
inline std::string tile_row(const std::uint32_t index,
                            const std::vector<std::string> &cells) {
  std::string storage;
  std::string offsets;
  for (const std::string &cell : cells) {
    const auto offset = cell.empty()
                            ? static_cast<std::int16_t>(-1)
                            : static_cast<std::int16_t>(storage.size());
    const auto bits = static_cast<std::uint16_t>(offset);
    offsets.push_back(static_cast<char>(bits & 0xff));
    offsets.push_back(static_cast<char>((bits >> 8) & 0xff));
    storage += cell;
  }

  return number_field(types::tile_row::index, index) +
         message_field(types::tile_row::storage, storage) +
         message_field(types::tile_row::offsets, offsets);
}

/// One row of a tile, stated as the bytes it carries rather than as its cells,
/// so a test can write an offsets table the storage does not back.
inline std::string tile_row_bytes(const std::uint32_t index,
                                  const std::string &storage,
                                  const std::vector<std::int16_t> &offsets) {
  std::string table;
  for (const std::int16_t offset : offsets) {
    const auto bits = static_cast<std::uint16_t>(offset);
    table.push_back(static_cast<char>(bits & 0xff));
    table.push_back(static_cast<char>((bits >> 8) & 0xff));
  }

  return number_field(types::tile_row::index, index) +
         message_field(types::tile_row::storage, storage) +
         message_field(types::tile_row::offsets, table);
}

/// A `TST.Tile` holding @p rows, each already built by @ref tile_row.
inline std::string tile(const std::vector<std::string> &rows) {
  std::string result;
  for (const std::string &row : rows) {
    result += message_field(types::tile::rows, row);
  }
  return result;
}

/// One entry of a `TST.DataList`: a key and the string it stands for.
inline std::string string_entry(const std::uint64_t key,
                                const std::string &text) {
  return message_field(types::data_list::entries,
                       number_field(types::data_list_entry::key, key) +
                           message_field(types::data_list_entry::string, text));
}

/// What a synthetic table is made of. The extent is what the model *declares*,
/// which the tiles need not fill — that is the shape the budget guards.
struct TableSpec final {
  std::string name;
  std::uint32_t rows{};
  std::uint32_t columns{};
  /// Rows already built by @ref tile_row, all in one tile.
  std::vector<std::string> tile_rows;
  /// Entries already built by @ref string_entry.
  std::vector<std::string> strings;
  /// How many times the tile list names the one tile.
  std::size_t tile_repeats{1};
  /// When set, a rich text list holding one entry under key 1, whose payload
  /// names this storage — which a `rich_text` cell reaches by that key.
  std::optional<std::uint64_t> rich_text_storage;
  /// When set, the tile's payload verbatim, in place of @ref tile_rows.
  std::optional<std::string> raw_tile;
  /// When set, the tile storage's tile list verbatim, in place of the entries
  /// @ref tile_repeats would write.
  std::optional<std::string> raw_tile_list;
};

/// The identifiers a synthetic table's objects are filed under.
constexpr std::uint64_t table_info_identifier = 20;
constexpr std::uint64_t table_model_identifier = 21;
constexpr std::uint64_t table_tile_identifier = 22;
constexpr std::uint64_t table_strings_identifier = 23;
constexpr std::uint64_t table_rich_texts_identifier = 25;
constexpr std::uint64_t rich_text_payload_identifier = 26;

/// The objects a table is made of: a `TST.TableInfoArchive`, the model behind
/// it, its tile and its string list.
inline std::string table_objects(const TableSpec &spec) {
  std::string entries;
  for (const std::string &entry : spec.strings) {
    entries += entry;
  }

  std::string tiles = spec.raw_tile_list.value_or(std::string());
  for (std::size_t repeat = 0;
       !spec.raw_tile_list.has_value() && repeat < spec.tile_repeats;
       ++repeat) {
    tiles += message_field(types::tile_storage::tiles,
                           number_field(types::tile_storage_entry::index, 0) +
                               reference_field(types::tile_storage_entry::tile,
                                               table_tile_identifier));
  }

  std::string store =
      message_field(types::data_store::tiles, tiles) +
      reference_field(types::data_store::string_list, table_strings_identifier);
  std::string rich_text_objects;
  if (spec.rich_text_storage.has_value()) {
    store += reference_field(types::data_store::rich_text_list,
                             table_rich_texts_identifier);

    const std::string payload = reference_field(
        types::rich_text_payload::storage, *spec.rich_text_storage);
    const std::string list =
        message_field(types::data_list::entries,
                      number_field(types::data_list_entry::key, 1) +
                          reference_field(types::data_list_entry::rich_text,
                                          rich_text_payload_identifier));

    rich_text_objects =
        object(table_rich_texts_identifier,
               {{types::archive_type::data_list, list.size()}}, list) +
        object(rich_text_payload_identifier,
               {{types::archive_type::rich_text_payload, payload.size()}},
               payload);
  }

  const std::string model =
      message_field(types::table_model::data_store, store) +
      number_field(types::table_model::rows, spec.rows) +
      number_field(types::table_model::columns, spec.columns) +
      message_field(types::table_model::name, spec.name);
  const std::string info =
      reference_field(types::table_info::model, table_model_identifier);
  const std::string tile_payload = spec.raw_tile.value_or(tile(spec.tile_rows));

  return object(table_info_identifier,
                {{types::archive_type::table_info, info.size()}}, info) +
         object(table_model_identifier,
                {{types::archive_type::table_model, model.size()}}, model) +
         object(table_tile_identifier,
                {{types::archive_type::tile, tile_payload.size()}},
                tile_payload) +
         object(table_strings_identifier,
                {{types::archive_type::data_list, entries.size()}}, entries) +
         rich_text_objects;
}

/// The identifier a synthetic attachment is filed under.
constexpr std::uint64_t attachment_identifier = 24;

/// A `.pages` package whose body anchors @p spec as a table: the body text
/// carries a `U+FFFC` at @p anchor_index, the attachment run table names the
/// `TSWP.DrawableAttachmentArchive` there, and that names the table.
inline std::shared_ptr<internal::abstract::ReadableFilesystem>
pages_table_package(const TableSpec &spec,
                    const std::string &text = "\xef\xbf\xbc",
                    const std::uint64_t anchor_index = 0) {
  const std::string storage =
      message_field(types::text_storage::text, text) +
      message_field(
          types::text_storage::attachments,
          message_field(
              types::attribute_table::entries,
              number_field(types::attribute_table_entry::character_index,
                           anchor_index) +
                  reference_field(types::attribute_table_entry::object,
                                  attachment_identifier)));

  const std::string attachment = reference_field(
      types::attachment_archive::drawable, table_info_identifier);

  const std::string root = document_archive(body_identifier);
  const std::string document =
      object(1, {{types::archive_type::pages_document, root.size()}}, root) +
      object(body_identifier,
             {{types::archive_type::text_storage, storage.size()}}, storage) +
      object(attachment_identifier,
             {{types::archive_type::drawable_attachment, attachment.size()}},
             attachment) +
      table_objects(spec);

  return package({{"Document", document}});
}

/// A `.numbers` package: a root archive naming one sheet, which names @p spec
/// as its only drawable. @p table_repeats is how many times the sheet's
/// drawable list names that one table.
inline std::shared_ptr<internal::abstract::ReadableFilesystem>
numbers_package(const TableSpec &spec, const std::string &sheet_name = "Sheet",
                const std::size_t table_repeats = 1) {
  constexpr std::uint64_t sheet_identifier = 2;

  const std::string root =
      reference_field(types::document_archive::sheets, sheet_identifier);

  std::string sheet = message_field(types::sheet_archive::name, sheet_name);
  for (std::size_t repeat = 0; repeat < table_repeats; ++repeat) {
    sheet +=
        reference_field(types::sheet_archive::drawables, table_info_identifier);
  }

  const std::string document =
      object(1, {{types::archive_type::app_document, root.size()}}, root) +
      object(sheet_identifier,
             {{types::archive_type::numbers_sheet, sheet.size()}}, sheet) +
      table_objects(spec);

  return package({{"Document", document}});
}

} // namespace odr::test::iwork
