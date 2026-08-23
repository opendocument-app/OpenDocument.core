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
namespace archive_type {
/// `TP.DocumentArchive`, the root of a `.pages` package.
/// `empty.pages Index/Document.iwa` object 1 (iWork 13.2).
constexpr std::uint32_t pages_document = 10000;
/// `TSWP.StorageArchive`, a run of text with its run tables.
/// `empty.pages Index/Document.iwa` object 1732514 (iWork 13.2).
constexpr std::uint32_t text_storage = 2001;
} // namespace archive_type

namespace document_archive {
/// The body text storage, as a `TSP.Reference`.
constexpr std::uint32_t body_storage = 4;
} // namespace document_archive

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
