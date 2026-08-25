#pragma once

#include <cstdint>

namespace odr::internal::iwork {

/// The archive types and field numbers the engine reads.
///
/// There is no spec and Apple has never published the `.proto` schemas, so
/// each of these is cited to the fixture it was read off rather than to a
/// section number, and holds for the iWork version that wrote it — see
/// `Metadata/BuildVersionHistory.plist`. A type id that is not here is one we
/// have not mapped, which the reader skips rather than throws on.
///
/// Type ids are namespaced per app: `TP.*` (Pages) starts at 10000 while
/// `KN.*` (Keynote) and `TN.*` (Numbers) both start at 1, so a `.key` and a
/// `.numbers` share every low id — see `app_by_components` in
/// `iwork_file.cpp`. Only the `TS*` frameworks (text storage, drawables,
/// tables) mean the same thing in all three.
namespace archive_type {
/// `TP.DocumentArchive`, the root of a `.pages` package.
/// `empty.pages Index/Document.iwa` object 1 (iWork 13.2).
constexpr std::uint32_t pages_document = 10000;
/// `KN.DocumentArchive` and `TN.DocumentArchive`, the root of a `.key` and of
/// a `.numbers` package alike.
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
/// `KN.PlaceholderArchive`, a title or body box a slide inherits from its
/// master. Wraps a @ref text_shape in its first field.
/// `empty.key Index/Slide-31724.iwa` object 31730 (iWork 14.4).
constexpr std::uint32_t keynote_placeholder = 7;
/// `TSWP.StorageArchive`, a run of text with its run tables.
/// `empty.pages Index/Document.iwa` object 1732514 (iWork 13.2).
constexpr std::uint32_t text_storage = 2001;
/// `TSWP.ShapeArchive`, a text box: a drawable plus the storage it holds.
/// `style-various-1.key Index/Slide-32281.iwa` object 32369 (iWork 14.4).
constexpr std::uint32_t text_shape = 2011;
} // namespace archive_type

namespace document_archive {
/// The body text storage, as a `TSP.Reference`. `TP.DocumentArchive` only.
constexpr std::uint32_t body_storage = 4;
/// The `KN.ShowArchive`, as a `TSP.Reference`. `KN.DocumentArchive` only.
constexpr std::uint32_t show = 2;
} // namespace document_archive

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
/// One drawable on the slide, as a `TSP.Reference`; repeated, in z-order.
/// Placeholders the slide leaves empty are not in this list, which is why it
/// is read rather than the title and body references beside it.
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

namespace text_storage {
/// The text, in a small number of large strings.
constexpr std::uint32_t text = 3;
/// The paragraph style run table: one entry per paragraph, holding the
/// character index the paragraph starts at and, where it has one, its style.
constexpr std::uint32_t paragraph_styles = 5;
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
} // namespace attribute_table_entry

} // namespace odr::internal::iwork
