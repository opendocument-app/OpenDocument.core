#pragma once

#include <cstddef>
#include <cstdint>

namespace odr::internal::iwork {

/// Fixture-verified archive IDs and fields. IDs are app-specific; see
/// AGENTS.md.
namespace archive_type {
/// `TP.DocumentArchive`, the root of a `.pages` package.
/// `empty.pages Index/Document.iwa` object 1 (iWork 13.2).
constexpr std::uint32_t pages_document = 10000;
/// `KN.DocumentArchive` / `TN.DocumentArchive`.
/// `empty.key` / `empty.numbers Index/Document.iwa` object 1 (iWork 14.4).
constexpr std::uint32_t app_document = 1;
/// `KN.ShowArchive`, the deck below a `.key` root.
/// `empty.key Index/Document.iwa` object 32004 (iWork 14.4).
constexpr std::uint32_t keynote_show = 2;
/// `KN.SlideNodeArchive`, one entry of the show's slide tree.
/// `empty.key Index/Document.iwa` object 31723 (iWork 14.4).
constexpr std::uint32_t keynote_slide_node = 4;
/// `KN.SlideArchive`, one slide.
/// `empty.key Index/Slide-31724.iwa` object 31724 (iWork 14.4).
constexpr std::uint32_t keynote_slide = 5;
/// `KN.PlaceholderArchive`, wrapping a text shape.
/// `empty.key Index/Slide-31724.iwa` object 31730 (iWork 14.4).
constexpr std::uint32_t keynote_placeholder = 7;
/// `TSWP.StorageArchive`, a run of text with its run tables.
/// `empty.pages Index/Document.iwa` object 1732514 (iWork 13.2).
constexpr std::uint32_t text_storage = 2001;
/// `TSWP.ShapeArchive`, a text box: a drawable plus the storage it holds.
/// `style-various-1.key Index/Slide-32281.iwa` object 32369 (iWork 14.4).
constexpr std::uint32_t text_shape = 2011;
/// `TSWP.DrawableAttachmentArchive`, what a `U+FFFC` in a text flow points at.
/// `style-various-1.pages Index/Document.iwa` object 1732925 (iWork 13.2).
constexpr std::uint32_t drawable_attachment = 2003;
/// `TN.SheetArchive`.
/// `style-various-1.numbers Index/Document.iwa` object 904475 (iWork 14.4).
constexpr std::uint32_t numbers_sheet = 2;
/// `TST.TableInfoArchive`, the drawable a table is placed by.
/// `style-various-1.pages Index/Document.iwa` object 1732842 (iWork 13.2).
constexpr std::uint32_t table_info = 6000;
/// `TST.TableModelArchive`, a table's extent, name and cell storage.
/// `style-various-1.pages Index/Document.iwa` object 1732845 (iWork 13.2).
constexpr std::uint32_t table_model = 6001;
/// `TST.Tile`, a range of rows holding packed cell records.
/// `style-various-1.pages Index/Tables/Tile.iwa` object 1732818 (iWork 13.2).
constexpr std::uint32_t tile = 6002;
/// `TST.DataList`, a side table cells reference by key.
/// `style-various-1.numbers Index/Tables/DataList-904489-2.iwa` (iWork 14.4).
constexpr std::uint32_t data_list = 6005;
/// `TST.RichTextPayloadArchive`, one entry of a rich text data list.
/// `style-various-1.pages Index/Document.iwa` object 1732965 (iWork 13.2).
constexpr std::uint32_t rich_text_payload = 6218;
} // namespace archive_type

namespace document_archive {
/// The body text storage, as a `TSP.Reference`. `TP.DocumentArchive` only.
constexpr std::uint32_t body_storage = 4;
/// The `KN.ShowArchive`, as a `TSP.Reference`. `KN.DocumentArchive` only.
constexpr std::uint32_t show = 2;
/// One `TN.SheetArchive`, as a `TSP.Reference`; repeated, in tab order.
/// `TN.DocumentArchive` only.
constexpr std::uint32_t sheets = 1;
} // namespace document_archive

namespace sheet_archive {
constexpr std::uint32_t name = 1;
/// One drawable on the sheet, as a `TSP.Reference`; repeated. A `.numbers`
/// sheet holds its tables here, one per table.
constexpr std::uint32_t drawables = 2;
} // namespace sheet_archive

namespace show_archive {
/// The slide tree, holding the deck's slides in presentation order.
constexpr std::uint32_t slide_tree = 3;
/// The slide size, as a `TSP.Size` in points.
constexpr std::uint32_t size = 4;
} // namespace show_archive

namespace slide_tree {
/// One `KN.SlideNodeArchive`, as a `TSP.Reference`; repeated.
constexpr std::uint32_t nodes = 2;
} // namespace slide_tree

namespace slide_node {
/// The `KN.SlideArchive` this node stands for, as a `TSP.Reference`.
constexpr std::uint32_t slide = 2;
} // namespace slide_node

namespace slide_archive {
/// Drawables in z-order as repeated `TSP.Reference`s; excludes empty
/// placeholders.
constexpr std::uint32_t drawables = 7;
} // namespace slide_archive

/// `KN.PlaceholderArchive` — a @ref archive_type::text_shape and the kind of
/// placeholder it stands for.
namespace placeholder_archive {
constexpr std::uint32_t shape = 1;
} // namespace placeholder_archive

/// `TSWP.ShapeArchive` — the drawable that carries a text storage.
namespace text_shape {
constexpr std::uint32_t shape = 1;
constexpr std::uint32_t storage = 2;
} // namespace text_shape

/// `TSD.ShapeArchive` — a drawable plus the path it is drawn along.
namespace shape_archive {
constexpr std::uint32_t drawable = 1;
} // namespace shape_archive

/// `TSD.DrawableArchive` — where a shape sits on its page.
namespace drawable_archive {
constexpr std::uint32_t geometry = 1;
} // namespace drawable_archive

/// `TSD.GeometryArchive` — position and size in points, both `TSP.Point`.
namespace geometry_archive {
constexpr std::uint32_t position = 1;
constexpr std::uint32_t size = 2;
} // namespace geometry_archive

namespace point {
constexpr std::uint32_t x = 1;
constexpr std::uint32_t y = 2;
} // namespace point

/// `TST.TableInfoArchive` — where a table sits, and the model behind it.
namespace table_info {
constexpr std::uint32_t model = 2;
} // namespace table_info

/// `TST.TableModelArchive` — a table's extent, its name and its cell storage.
namespace table_model {
constexpr std::uint32_t data_store = 4;
constexpr std::uint32_t rows = 6;
constexpr std::uint32_t columns = 7;
constexpr std::uint32_t name = 8;
} // namespace table_model

/// `TST.DataStore`: tiles and the side lists referenced by cells.
namespace data_store {
constexpr std::uint32_t tiles = 3;
constexpr std::uint32_t string_list = 4;
constexpr std::uint32_t rich_text_list = 17;
} // namespace data_store

namespace tile_storage {
/// One `{index, tile}` pair; repeated.
constexpr std::uint32_t tiles = 1;
/// How many rows a tile covers, so a tile's row index is relative to it.
constexpr std::uint32_t rows_per_tile = 2;
} // namespace tile_storage

namespace tile_storage_entry {
constexpr std::uint32_t index = 1;
constexpr std::uint32_t tile = 2;
} // namespace tile_storage_entry

namespace tile {
/// One `TST.TileRowInfo`; repeated, only for rows that hold something.
constexpr std::uint32_t rows = 5;
} // namespace tile

/// `TST.TileRowInfo` — one row of a tile. Fields 3 and 4 carry an older
/// encoding of the same cells, which @ref cell::version says not to read.
namespace tile_row {
constexpr std::uint32_t index = 1;
constexpr std::uint32_t storage = 6;
/// `std::int16_t` per column, the offset of that column's cell into
/// @ref storage, or `-1` where the row holds no cell there.
constexpr std::uint32_t offsets = 7;
} // namespace tile_row

namespace data_list {
constexpr std::uint32_t entries = 3;
} // namespace data_list

namespace data_list_entry {
constexpr std::uint32_t key = 1;
constexpr std::uint32_t string = 3;
constexpr std::uint32_t rich_text = 9;
} // namespace data_list_entry

namespace rich_text_payload {
constexpr std::uint32_t storage = 1;
} // namespace rich_text_payload

/// Packed cell layout, verified on `style-various-1.pages` (iWork 13.2) and
/// `style-various-1.numbers` (iWork 14.4). See AGENTS.md for the byte layout.
namespace cell {
/// The only encoding both fixtures write. A record that declares another is
/// one we have not mapped, and reads as an empty cell rather than a wrong one.
constexpr std::uint8_t version = 5;
constexpr std::size_t header_size = 12;
constexpr std::size_t version_offset = 0;
constexpr std::size_t type_offset = 1;
constexpr std::size_t flags_offset = 8;

/// A cell's type byte.
namespace type {
constexpr std::uint8_t number = 2;    ///< a decimal128 in @ref flag::decimal
constexpr std::uint8_t string = 3;    ///< a key into the string list
constexpr std::uint8_t date = 5;      ///< seconds since 2001-01-01T00:00:00Z
constexpr std::uint8_t boolean = 6;   ///< `1.0` or `0.0`
constexpr std::uint8_t duration = 7;  ///< a count of seconds
constexpr std::uint8_t rich_text = 9; ///< a key into the rich text list
} // namespace type

/// Value payload flags; higher bits describe unparsed styles and formats.
namespace flag {
constexpr std::uint32_t decimal = 1U << 0;       ///< 16 bytes, IEEE decimal128
constexpr std::uint32_t number = 1U << 1;        ///< 8 bytes, IEEE double
constexpr std::uint32_t seconds = 1U << 2;       ///< 8 bytes, IEEE double
constexpr std::uint32_t string_key = 1U << 3;    ///< 4 bytes
constexpr std::uint32_t rich_text_key = 1U << 4; ///< 4 bytes
} // namespace flag
} // namespace cell

namespace text_storage {
/// The text, in a small number of large strings.
constexpr std::uint32_t text = 3;
/// The paragraph style run table: one entry per paragraph, holding the
/// character index the paragraph starts at and, where it has one, its style.
constexpr std::uint32_t paragraph_styles = 5;
/// Attachments by UTF-16 index: `style-various-1.pages` body storage has
/// `U+FFFC` anchors at 18, 460 and 469 (iWork 13.2).
constexpr std::uint32_t attachments = 9;
} // namespace text_storage

/// A run table parallel to the text, as `TSWP.ObjectAttributeTable`.
namespace attribute_table {
constexpr std::uint32_t entries = 1;
} // namespace attribute_table

namespace reference {
constexpr std::uint32_t identifier = 1;
} // namespace reference

namespace attribute_table_entry {
constexpr std::uint32_t character_index = 1;
/// The object the entry attaches there, as a `TSP.Reference`.
constexpr std::uint32_t object = 2;
} // namespace attribute_table_entry

/// `TSWP.DrawableAttachmentArchive` — the drawable a text anchor stands for.
namespace attachment_archive {
constexpr std::uint32_t drawable = 1;
} // namespace attachment_archive

} // namespace odr::internal::iwork
