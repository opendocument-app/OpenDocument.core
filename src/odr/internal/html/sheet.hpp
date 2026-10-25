#pragma once

#include <odr/html.hpp>

#include <odr/internal/html/html_writer.hpp>

#include <optional>
#include <string>

namespace odr {
class Document;
class Sheet;
} // namespace odr

namespace odr::internal::html {
struct WritingState;

/// A sheet as the ruler and a table for each chunk of its rows
/// (`lazy-loading.md` decision 3).
void translate_sheet(const Sheet &sheet, const WritingState &state);

/// The extent `translate_sheet` writes @p sheet at, once the limits in
/// @p config have applied.
[[nodiscard]] TableDimensions sheet_rendered_extent(const Sheet &sheet,
                                                    const HtmlConfig &config);

/// Costs a pass over the cells, so measure it once.
[[nodiscard]] std::optional<HtmlSheetCut> sheet_cut(const Sheet &sheet,
                                                    const HtmlConfig &config);

/// The class of the gridlines @p config draws, with a space before it.
[[nodiscard]] std::string sheet_body_classes(const HtmlConfig &config);
/// What the body of an editable sheet page states for the editor: the edit
/// gesture, the locale, and what the page computes a formula with.
void write_sheet_body_attributes(const Document &document,
                                 const WritingState &state,
                                 const HtmlAttributeWriterCallback &clb);
/// The scripts of a sheet page, and of its editor where the render edits.
void write_sheet_scripts(const WritingState &state);

} // namespace odr::internal::html
