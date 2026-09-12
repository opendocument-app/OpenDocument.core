#include <odr/internal/common/sheet_dependencies.hpp>

#include <odr/file.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/formula/formula_dependencies.hpp>
#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <optional>
#include <string>

namespace odr::internal {

namespace {

/// The syntax the engine behind @p file_type writes a formula in. Nothing
/// where it states none at all.
std::optional<formula::Syntax> syntax_of(const FileType file_type) {
  switch (file_type) {
  case FileType::opendocument_spreadsheet:
    return formula::Syntax::opendocument;
  case FileType::office_open_xml_workbook:
    return formula::Syntax::ooxml;
  default:
    return {};
  }
}

} // namespace

bool SheetDependencies::Read::contains(const SheetPosition &position) const {
  return position.sheet == sheet && range.contains(position.cell);
}

SheetDependencies SheetDependencies::of(const abstract::Document &document) {
  SheetDependencies result;

  const std::optional<formula::Syntax> syntax = syntax_of(document.file_type());
  const abstract::ElementAdapter *adapter = document.element_adapter();
  if (!syntax.has_value() || adapter == nullptr) {
    return result;
  }

  std::vector<ElementIdentifier> sheets;
  std::unordered_map<std::string, std::uint32_t> by_name;
  for (ElementIdentifier id =
           adapter->element_first_child(document.root_element());
       id != null_element_id; id = adapter->element_next_sibling(id)) {
    if (adapter->element_type(id) != ElementType::sheet) {
      continue;
    }
    // a formula names a sheet without case, as the applications writing one do
    by_name.emplace(
        util::string::to_lower(adapter->sheet_adapter(id)->sheet_name(id)),
        static_cast<std::uint32_t>(sheets.size()));
    sheets.push_back(id);
  }

  for (std::uint32_t index = 0; index < sheets.size(); ++index) {
    const ElementIdentifier sheet_id = sheets[index];
    adapter->sheet_adapter(sheet_id)->sheet_visit_formulas(
        sheet_id, [&](const std::uint32_t column, const std::uint32_t row,
                      const std::string &formula) {
          if (!formula.empty()) {
            result.add_formula_(SheetPosition(index, column, row), formula,
                                *syntax, by_name);
          }
        });
  }

  // a sheet hands its cells out in whatever order it holds them
  std::ranges::sort(result.m_unresolved);
  return result;
}

void SheetDependencies::add_formula_(
    const SheetPosition &cell, const std::string &expression,
    const formula::Syntax syntax,
    const std::unordered_map<std::string, std::uint32_t> &by_name) {
  const std::optional<formula::Node> node = formula::parse(expression, syntax);
  if (!node.has_value()) {
    m_unresolved.push_back(cell);
    return;
  }

  const formula::References references = formula::references(*node);
  Entry entry;
  entry.cell = cell;
  bool complete = references.complete;
  for (const formula::Extent &extent : references.extents) {
    if (extent.document.has_value()) {
      continue; // another file, which nothing here opens and no edit reaches
    }
    std::uint32_t sheet = cell.sheet;
    if (extent.sheet.has_value()) {
      const auto named = by_name.find(util::string::to_lower(*extent.sheet));
      if (named == by_name.end()) {
        complete = false; // a sheet this document has none of, or a span
        continue;
      }
      sheet = named->second;
    }
    entry.reads.push_back(Read{sheet, extent.range});
  }

  if (!complete) {
    m_unresolved.push_back(cell);
  }
  if (entry.reads.empty()) {
    return;
  }

  const std::size_t at = m_entries.size();
  for (const Read &read : entry.reads) {
    std::vector<std::size_t> &bucket = m_by_sheet[read.sheet];
    if (bucket.empty() || bucket.back() != at) {
      bucket.push_back(at);
    }
  }
  m_entries.push_back(std::move(entry));
}

std::vector<SheetPosition> SheetDependencies::dependents(
    const std::vector<SheetPosition> &positions) const {
  std::vector<bool> reported(m_entries.size(), false);
  std::vector<SheetPosition> frontier = positions;
  std::vector<SheetPosition> result;

  while (!frontier.empty()) {
    std::vector<SheetPosition> next;
    for (const SheetPosition &position : frontier) {
      const auto bucket = m_by_sheet.find(position.sheet);
      if (bucket == m_by_sheet.end()) {
        continue;
      }
      for (const std::size_t at : bucket->second) {
        if (reported[at]) {
          continue;
        }
        const Entry &entry = m_entries[at];
        if (!std::ranges::any_of(entry.reads, [&](const Read &read) {
              return read.contains(position);
            })) {
          continue;
        }
        reported[at] = true;
        result.push_back(entry.cell);
        next.push_back(entry.cell);
      }
    }
    frontier = std::move(next);
  }

  std::ranges::sort(result);
  return result;
}

const std::vector<SheetPosition> &
SheetDependencies::unresolved() const noexcept {
  return m_unresolved;
}

} // namespace odr::internal
