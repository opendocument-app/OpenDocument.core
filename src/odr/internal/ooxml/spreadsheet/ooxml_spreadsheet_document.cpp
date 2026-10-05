#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/table_position.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/element_adapter.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/table_range.hpp>
#include <odr/internal/formula/formula_ast.hpp>
#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/formula/formula_value.hpp>
#include <odr/internal/formula/formula_writer.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_references.hpp>
#include <odr/internal/util/number_util.hpp>
#include <odr/internal/xml/xml_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ostream>
#include <ranges>
#include <sstream>
#include <string_view>
#include <utility>

#include <fmt/format.h>

namespace odr::internal::ooxml::spreadsheet {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(Document &document, ElementRegistry &registry);

/// The `workbook` `calcPr`, appended where it is missing. ECMA-376 18.2.27
/// orders the children, so a new one goes before the first that must follow it.
pugi::xml_node calc_pr(pugi::xml_node workbook) {
  if (const pugi::xml_node existing = workbook.child("calcPr")) {
    return existing;
  }
  static constexpr std::array<std::string_view, 9> after = {
      "oleSize",        "customWorkbookViews", "pivotCaches",
      "smartTagPr",     "smartTagTypes",       "webPublishing",
      "fileRecoveryPr", "webPublishObjects",   "extLst"};
  for (const pugi::xml_node child : workbook.children()) {
    if (std::ranges::find(after, std::string_view(child.name())) !=
        std::end(after)) {
      return workbook.insert_child_before("calcPr", child);
    }
  }
  return workbook.append_child("calcPr");
}
} // namespace

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> files)
    : internal::Document(FileType::office_open_xml_workbook,
                         DocumentType::spreadsheet, std::move(files)) {
  const AbsPath workbook_path("/xl/workbook.xml");
  const auto [workbook_xml, workbook_relations] = parse_xml_(workbook_path);
  m_written_parts.push_back(workbook_path);
  if (workbook_xml.document_element()
          .child("workbookPr")
          .attribute("date1904")
          .as_bool()) {
    m_epoch = number_format::Epoch::from_1904;
  }
  const AbsPath styles_path("/xl/styles.xml");
  const auto [styles_xml, _] = parse_xml_(styles_path);
  m_written_parts.push_back(styles_path);

  for (pugi::xml_node sheet_node :
       workbook_xml.document_element().child("sheets").children("sheet")) {
    const char *id = sheet_node.attribute("r:id").value();
    const AbsPath sheet_path =
        workbook_path.parent().join(RelPath(workbook_relations.at(id)));
    const auto [sheet_xml, sheet_relationships] = parse_xml_(sheet_path);
    m_written_parts.push_back(sheet_path);

    if (const pugi::xml_node drawing =
            sheet_xml.document_element().child("drawing")) {
      const AbsPath drawing_path = sheet_path.parent().join(
          RelPath(sheet_relationships.at(drawing.attribute("r:id").value())));
      parse_xml_(drawing_path);
    }
  }

  if (m_files->exists(AbsPath("/xl/sharedStrings.xml"))) {
    const auto [shared_strings_xml, _] =
        parse_xml_(AbsPath("/xl/sharedStrings.xml"));
    for (const pugi::xml_node shared_string :
         shared_strings_xml.document_element()) {
      m_shared_strings.push_back(shared_string);
    }
  }

  pugi::xml_node theme_root;
  if (const std::optional<AbsPath> theme_path =
          parse_relationship_target(*m_files, workbook_path, "theme");
      theme_path && m_files->is_file(*theme_path)) {
    theme_root = parse_xml_(*theme_path).first.document_element();
  }
  m_style_registry = StyleRegistry(styles_xml.document_element(), theme_root);

  const ParseContext parse_context(workbook_path, workbook_relations,
                                   m_xml_documents_and_relations,
                                   m_shared_strings);
  m_root_element = parse_tree(m_element_registry, parse_context,
                              workbook_xml.document_element());

  m_element_adapter = create_element_adapter(*this, m_element_registry);
}

const ElementRegistry &Document::element_registry() const {
  return m_element_registry;
}

const StyleRegistry &Document::style_registry() const {
  return m_style_registry;
}

StyleRegistry &Document::style_registry() { return m_style_registry; }

number_format::Epoch Document::epoch() const { return m_epoch; }

std::vector<formula::Name> Document::formula_names() const {
  std::vector<formula::Name> result;
  // ECMA-376 18.2.5: a name local to a sheet states its index among them
  for (const pugi::xml_node name :
       workbook().child("definedNames").children("definedName")) {
    std::optional<std::uint32_t> sheet;
    if (const pugi::xml_attribute local = name.attribute("localSheetId")) {
      sheet = local.as_uint();
    }
    result.push_back(formula::Name{name.attribute("name").value(), sheet,
                                   name.text().get()});
  }
  return result;
}

formula::Settings Document::formula_settings() const {
  return formula::Settings{.epoch = m_epoch};
}

pugi::xml_node Document::workbook() const {
  return m_xml_documents_and_relations.at(AbsPath("/xl/workbook.xml"))
      .first.document_element();
}

pugi::xml_node Document::part(const AbsPath &path) {
  if (!m_files->is_file(path)) {
    return {};
  }
  auto parsed = m_xml_documents_and_relations.find(path);
  const pugi::xml_node root = parsed != m_xml_documents_and_relations.end()
                                  ? parsed->second.first.document_element()
                                  : parse_xml_(path).first.document_element();
  // after the parse, so a part that does not parse is never written
  if (std::ranges::find(m_written_parts, path) == m_written_parts.end()) {
    m_written_parts.push_back(path);
  }
  return root;
}

std::vector<pugi::xml_node>
Document::related_parts(const AbsPath &origin, const std::string_view type) {
  std::vector<pugi::xml_node> result;
  for (const AbsPath &path :
       parse_relationship_targets(*m_files, origin, type)) {
    if (const pugi::xml_node root = part(path)) {
      result.push_back(root);
    }
  }
  return result;
}

const Relations &Document::relations_of(const AbsPath &path) const {
  return m_xml_documents_and_relations.at(path).second;
}

pugi::xml_node Document::related_part(const AbsPath &origin,
                                      const std::string_view type) {
  const std::optional<AbsPath> path =
      parse_relationship_target(*m_files, origin, type);
  return path.has_value() ? part(*path) : pugi::xml_node();
}

bool Document::is_editable() const noexcept { return true; }

bool Document::is_savable(const bool encrypted) const noexcept {
  return !encrypted && !is_decrypted();
}

void Document::save(std::ostream &out) const {
  if (!is_savable(false)) {
    throw UnsupportedOperation();
  }

  // ECMA-376 18.2.2: nothing here computes a formula, so every save asks the
  // reader to recompute what this one may have invalidated
  pugi::xml_node calc_node = calc_pr(workbook());
  calc_node.remove_attribute("fullCalcOnLoad");
  calc_node.append_attribute("fullCalcOnLoad").set_value("1");

  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipArchive archive;

  for (auto walker = m_files->file_walker(AbsPath("/")); !walker->end();
       walker->next()) {
    const AbsPath &abs_path = walker->path();
    RelPath rel_path = abs_path.rebase(AbsPath("/"));
    if (walker->is_directory()) {
      archive.insert_directory(std::end(archive), rel_path);
      continue;
    }
    if (std::ranges::find(m_written_parts, abs_path) !=
        std::end(m_written_parts)) {
      // TODO stream
      std::stringstream content;
      // pugixml is never asked to parse the declaration, so it writes none back
      content << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)";
      m_xml_documents_and_relations.at(abs_path).first.print(content, "",
                                                             pugi::format_raw);
      auto tmp = std::make_shared<MemoryFile>(content.str());
      archive.insert_file(std::end(archive), rel_path, tmp);
      continue;
    }
    archive.insert_file(std::end(archive), rel_path, m_files->open(abs_path));
  }

  archive.save(out);
}

void Document::save(std::ostream & /*out*/, const char * /*password*/) const {
  throw UnsupportedOperation();
}

std::pair<pugi::xml_document &, Relations &>
Document::parse_xml_(const AbsPath &path) {
  pugi::xml_document document = xml::parse(*m_files, path);
  Relations relations = parse_relationships(*m_files, path);

  auto [it, _] = m_xml_documents_and_relations.emplace(
      path, std::make_pair(std::move(document), std::move(relations)));
  return {it->second.first, it->second.second};
}

namespace {

using AdapterBase = internal::RegistryElementAdapter<
    ElementRegistry, abstract::SheetAdapter, abstract::SheetCellAdapter,
    abstract::LineBreakAdapter, abstract::ParagraphAdapter,
    abstract::SpanAdapter, abstract::TextAdapter, abstract::LinkAdapter,
    abstract::FrameAdapter, abstract::ImageAdapter>;

class ElementAdapter final : public AdapterBase {
public:
  ElementAdapter(Document &document, ElementRegistry &registry)
      : AdapterBase(registry), m_document(&document) {}

  [[nodiscard]] std::string
  sheet_name(const ElementIdentifier element_id) const override {
    return m_registry->sheet_element_at(element_id).name;
  }
  /// TODO `pageSetup` is not read; a column width here is a `ch` either way.
  [[nodiscard]] PageLayout sheet_page_layout(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }
  [[nodiscard]] TableDimensions
  sheet_dimensions(const ElementIdentifier element_id) const override {
    return m_registry->sheet_element_at(element_id).dimensions;
  }
  [[nodiscard]] TableDimensions
  sheet_content(const ElementIdentifier element_id,
                [[maybe_unused]] const std::optional<TableDimensions> range)
      const override {
    // TODO the range is ignored: this answers the whole `<dimension>` rather
    // than trimming to the populated cells inside it.
    return sheet_dimensions(element_id);
  }
  [[nodiscard]] ElementIdentifier
  sheet_cell(const ElementIdentifier element_id, const std::uint32_t column,
             const std::uint32_t row) const override {
    const ElementRegistry::Sheet &sheet_element =
        m_registry->sheet_element_at(element_id);
    if (const ElementRegistry::Sheet::Cell *cell =
            sheet_element.cell(column, row);
        cell != nullptr) {
      return cell->element_id;
    }
    return {};
  }
  [[nodiscard]] ElementIdentifier
  sheet_first_shape(const ElementIdentifier element_id) const override {
    return m_registry->sheet_element_at(element_id).first_shape_id;
  }
  /// The cell map, not the grid `dimension` claims: a sheet states a `c` for
  /// every cell it holds.
  void sheet_visit_formulas(
      const ElementIdentifier element_id,
      const abstract::SheetFormulaVisitor &visitor) const override {
    for (const auto &[position, cell] :
         m_registry->sheet_element_at(element_id).cells) {
      if (const pugi::xml_node formula = cell.node.child("f")) {
        // ECMA-376 18.3.1.40: an array formula states the range it fills
        const bool array =
            std::string_view(formula.attribute("t").value()) == "array";
        TableDimensions span(1, 1);
        if (const std::string ref = formula.attribute("ref").value();
            array && ref.find(':') != std::string::npos) {
          const TableRange range(ref);
          const auto [top, bottom] =
              std::minmax(range.from().row, range.to().row);
          const auto [left, right] =
              std::minmax(range.from().column, range.to().column);
          span = TableDimensions(bottom - top + 1, right - left + 1);
        }
        visitor(position.column, position.row, span, array,
                formula_expression(cell.element_id, formula));
      }
    }
  }
  bool
  sheet_visit_cells(const ElementIdentifier element_id,
                    const abstract::SheetCellVisitor &visitor) const override {
    for (const auto &[position, cell] :
         m_registry->sheet_element_at(element_id).cells) {
      visitor(position.column, position.row, TableDimensions(1, 1),
              cell.element_id);
    }
    return true;
  }
  /// ECMA-376 18.3.1.4: a cell states its value as `v`, or as the text under
  /// `is` with `t="inlineStr"`. A written string goes inline - rewriting the
  /// shared entry would rewrite every other cell indexing it.
  void sheet_set_cell(const ElementIdentifier element_id,
                      const std::uint32_t column, const std::uint32_t row,
                      const CellValue &value) const override {
    if (value.type() == ValueType::error ||
        ((value.type() == ValueType::date || value.type() == ValueType::time) &&
         (!value.has_number() || !std::isfinite(value.number())))) {
      throw UnsupportedOperation(); // no form to write it in
    }
    const ElementRegistry::Sheet &sheet =
        m_registry->sheet_element_at(element_id);
    const ElementRegistry::Sheet::Cell *cell = sheet.cell(column, row);

    ElementIdentifier cell_id = null_element_id;
    if (cell == nullptr) {
      // the file spells no `c` here, so there is nothing to refuse
      cell_id = insert_cell(element_id, column, row);
    } else {
      cell_id = cell->element_id;
      if (m_registry->sheet_cell_element_at(cell_id).is_covered) {
        throw UnsupportedOperation(); // the anchor of the merge answers for it
      }
      if (cell->node.child("f")) {
        throw UnsupportedOperation(); // its dependants would go stale
      }
    }

    pugi::xml_node node = get_node(cell_id);

    m_registry->invalidate_children(cell_id);
    node.remove_children();
    node.remove_attribute("t");

    switch (value.type()) {
    case ValueType::unknown:
      break;
    case ValueType::string: {
      node.append_attribute("t").set_value("inlineStr");
      pugi::xml_node text_node = node.append_child("is").append_child("t");
      // the text is written verbatim, so a leading space in one has to survive
      text_node.append_attribute("xml:space").set_value("preserve");
      text_node.text().set(value.has_text() ? value.text().c_str() : "");
      const auto &[text_id, unused1, unused2] =
          m_registry->create_text_element(text_node, text_node);
      m_registry->append_child(cell_id, text_id);
      // Excel shows a line break only in a cell that wraps
      if (value.has_text() && value.text().contains('\n') &&
          !sheet_cell_style(element_id, column, row)
               .wrap_text.value_or(false)) {
        TableCellStyle wraps;
        wraps.wrap_text = true;
        sheet_set_cell_style(element_id, column, row, wraps, {});
      }
    } break;
    case ValueType::float_number: {
      // `t` defaults to "n"; the number format, not a stored string, is
      // what shows the number, so `value.text()` has nowhere to go
      const pugi::xml_node value_node = node.append_child("v");
      value_node.text().set(fmt::format("{}", value.number()).c_str());
      const auto &[text_id, unused1, unused2] =
          m_registry->create_text_element(value_node, value_node);
      m_registry->append_child(cell_id, text_id);
    } break;
    case ValueType::boolean: {
      node.append_attribute("t").set_value("b");
      const pugi::xml_node value_node = node.append_child("v");
      value_node.text().set(value.has_number() && value.number() != 0 ? "1"
                                                                      : "0");
      const auto &[text_id, unused1, unused2] =
          m_registry->create_text_element(value_node, value_node);
      m_registry->append_child(cell_id, text_id);
    } break;
    case ValueType::date:
    case ValueType::time: {
      const pugi::xml_node value_node = node.append_child("v");
      // a time is a duration, which no epoch moves
      value_node.text().set(
          fmt::format("{}", value.type() == ValueType::time
                                ? value.number()
                                : number_format::serial_from_days(
                                      value.number(), m_document->epoch()))
              .c_str());
      const auto &[text_id, unused1, unused2] =
          m_registry->create_text_element(value_node, value_node);
      m_registry->append_child(cell_id, text_id);
      // a date typed into a cell of a number format gets a date format, as
      // Excel gives it one: 14, 20, 21, 22 or 46 of ECMA-376 18.8.30
      if (number_format_of(cell_id).category() ==
          number_format::Category::number) {
        const std::int64_t seconds =
            std::llround(std::fmod(std::abs(value.number()), 1) * 86400);
        const std::uint32_t id = value.type() == ValueType::time
                                     ? (std::abs(value.number()) >= 1 ? 46
                                        : seconds % 60 != 0           ? 21
                                                                      : 20)
                                 : seconds % 86400 != 0 ? 22
                                                        : 14;
        restyle_cell(node,
                     shown_format(node, m_registry->sheet_element_at(element_id)
                                            .column_node(column)),
                     {}, {}, id);
      }
    } break;
    case ValueType::error:
      throw UnsupportedOperation(); // refused above
    }
    m_document->note_written(element_id, TablePosition(column, row));
  }

  /// ECMA-376 18.3.1.4: the result is `v` after the `f`, typed by `t`: `str`
  /// for a text, `b` for a boolean, `e` for an error.
  void sheet_set_result(const ElementIdentifier element_id,
                        const std::uint32_t column, const std::uint32_t row,
                        const CellValue &result) const override {
    const ElementRegistry::Sheet::Cell *cell =
        m_registry->sheet_element_at(element_id).cell(column, row);
    if (cell == nullptr || !cell->node.child("f")) {
      throw UnsupportedOperation();
    }
    // a stale result stays, as every save sets `fullCalcOnLoad`
    if (result.type() == ValueType::unknown) {
      return;
    }
    const ElementIdentifier cell_id = cell->element_id;
    pugi::xml_node node = get_node(cell_id);
    m_registry->invalidate_children(cell_id);
    for (const char *stated : {"v", "is"}) {
      while (const pugi::xml_node child = node.child(stated)) {
        node.remove_child(child);
      }
    }
    node.remove_attribute("t");

    std::string text;
    switch (result.type()) {
    case ValueType::float_number:
    case ValueType::date:
    case ValueType::time:
      text = fmt::format("{}", result.number());
      break;
    case ValueType::string:
      node.append_attribute("t").set_value("str");
      text = result.has_text() ? result.text() : "";
      break;
    case ValueType::boolean:
      node.append_attribute("t").set_value("b");
      text = result.has_number() && result.number() != 0 ? "1" : "0";
      break;
    case ValueType::error:
      node.append_attribute("t").set_value("e");
      text = result.has_text() ? result.text() : "#VALUE!";
      break;
    case ValueType::unknown:
      break;
    }
    const pugi::xml_node value_node =
        node.insert_child_after("v", node.child("f"));
    value_node.text().set(text.c_str());
    const auto &[text_id, unused1, unused2] =
        m_registry->create_text_element(value_node, value_node);
    m_registry->append_child(cell_id, text_id);
  }

  /// TODO a sheet carries no style of its own here; `sheetFormatPr` (default
  /// row height and column width) is not read.
  [[nodiscard]] TableStyle sheet_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }
  [[nodiscard]] TableColumnStyle
  sheet_column_style(const ElementIdentifier element_id,
                     const std::uint32_t column) const override {
    const ElementRegistry::Sheet &sheet_element =
        m_registry->sheet_element_at(element_id);
    const pugi::xml_node column_node = sheet_element.column_node(column);

    TableColumnStyle result;
    if (const pugi::xml_attribute width = column_node.attribute("width")) {
      result.width = Measure(width.as_float(), DynamicUnit("ch"));
    }
    return result;
  }
  [[nodiscard]] TableRowStyle
  sheet_row_style(const ElementIdentifier element_id,
                  const std::uint32_t row) const override {
    const ElementRegistry::Sheet &sheet_element =
        m_registry->sheet_element_at(element_id);
    const pugi::xml_node row_node = sheet_element.row_node(row);

    TableRowStyle result;
    if (const pugi::xml_attribute height = row_node.attribute("ht")) {
      result.height = Measure(height.as_float(), DynamicUnit("pt"));
    }
    return result;
  }
  /// [ECMA-376] 18.3.1.4: a `c` without `s` shows its row's `s` where the
  /// row states `customFormat`, else its column's `style`.
  void sheet_set_cell_style(const ElementIdentifier element_id,
                            const std::uint32_t column, const std::uint32_t row,
                            const TableCellStyle &cell_style,
                            const TextStyle &text_style) const override {
    const ElementRegistry::Sheet &sheet =
        m_registry->sheet_element_at(element_id);
    const ElementRegistry::Sheet::Cell *cell = sheet.cell(column, row);

    ElementIdentifier cell_id = null_element_id;
    if (cell == nullptr) {
      cell_id = insert_cell(element_id, column, row);
    } else {
      cell_id = cell->element_id;
      if (m_registry->sheet_cell_element_at(cell_id).is_covered) {
        throw UnsupportedOperation();
      }
    }

    pugi::xml_node node = get_node(cell_id);
    restyle_cell(
        node,
        shown_format(
            node, m_registry->sheet_element_at(element_id).column_node(column)),
        cell_style, text_style);
  }

  /// Applies row and cell styles; materializes missing cells where the new
  /// row style would hide a column style.
  void sheet_set_row_style(const ElementIdentifier element_id,
                           const std::uint32_t row,
                           const TableCellStyle &cell_style,
                           const TextStyle &text_style) const override {
    ElementRegistry::Sheet &sheet = m_registry->sheet_element_at(element_id);
    const pugi::xml_node sheet_node = get_node(element_id);

    pugi::xml_node row_node = sheet.row_node(row);
    if (!row_node) {
      pugi::xml_node sheet_data = sheet_node.child("sheetData");
      if (!sheet_data) {
        throw UnsupportedOperation(); // 18.3.1.99 states one for every sheet
      }
      row_node = insert_row_node(sheet_data, row);
      row_node.append_attribute("r").set_value(row + 1);
      sheet.register_row(row, row_node);
    }
    const bool row_formatted = row_node.attribute("customFormat").as_bool();

    for (const pugi::xml_node cell : row_node.children("c")) {
      const TablePosition position(cell.attribute("r").value());
      restyle_cell(cell, shown_format(cell, sheet.column_node(position.column)),
                   cell_style, text_style);
    }
    if (!row_formatted) {
      for (const auto &[max, entry] : sheet.columns) {
        const std::uint32_t style = entry.node.attribute("style").as_uint();
        for (std::uint32_t column = entry.min;
             style != 0 && column <= max && column < sheet.dimensions.columns;
             ++column) {
          if (sheet.cell(column, row) == nullptr &&
              !is_covered_by_merge(sheet_node, {column, row})) {
            restyle_cell(get_node(insert_cell(element_id, column, row)), style,
                         cell_style, text_style);
          }
        }
      }
    }

    xml::set_attribute(
        row_node, "s",
        std::to_string(
            m_document->style_registry().create_cell_format(
                row_formatted ? row_node.attribute("s").as_uint() : 0,
                cell_style, text_style))
            .c_str());
    xml::set_attribute(row_node, "customFormat", "1");
  }

  /// Applies column and cell styles; materializes missing cells where a row
  /// style would hide the new column style.
  void sheet_set_column_style(const ElementIdentifier element_id,
                              const std::uint32_t column,
                              const TableCellStyle &cell_style,
                              const TextStyle &text_style) const override {
    ElementRegistry::Sheet &sheet = m_registry->sheet_element_at(element_id);
    const pugi::xml_node sheet_node = get_node(element_id);
    const pugi::xml_node column_node = claim_column(sheet_node, sheet, column);

    for (const auto &[row, entry] : sheet.rows) {
      if (const ElementRegistry::Sheet::Cell *cell = sheet.cell(column, row);
          cell != nullptr) {
        restyle_cell(cell->node, shown_format(cell->node, column_node),
                     cell_style, text_style);
      } else if (entry.node.attribute("customFormat").as_bool() &&
                 !is_covered_by_merge(sheet_node, {column, row})) {
        restyle_cell(get_node(insert_cell(element_id, column, row)),
                     entry.node.attribute("s").as_uint(), cell_style,
                     text_style);
      }
    }

    xml::set_attribute(
        column_node, "style",
        std::to_string(m_document->style_registry().create_cell_format(
                           column_node.attribute("style").as_uint(), cell_style,
                           text_style))
            .c_str());
  }

  void sheet_insert_rows(const ElementIdentifier element_id,
                         const std::uint32_t row,
                         const std::uint32_t count) const override {
    edit_sheet(element_id, {.index = row, .count = count});
  }
  void sheet_delete_rows(const ElementIdentifier element_id,
                         const std::uint32_t row,
                         const std::uint32_t count) const override {
    edit_sheet(element_id, {.index = row, .count = count, .insert = false});
  }
  void sheet_insert_columns(const ElementIdentifier element_id,
                            const std::uint32_t column,
                            const std::uint32_t count) const override {
    edit_sheet(
        element_id,
        {.axis = formula::Axis::column, .index = column, .count = count});
  }
  void sheet_delete_columns(const ElementIdentifier element_id,
                            const std::uint32_t column,
                            const std::uint32_t count) const override {
    edit_sheet(element_id, {.axis = formula::Axis::column,
                            .index = column,
                            .count = count,
                            .insert = false});
  }

  /// The rows and the columns of the grid, `1048576` and `XFD`.
  static constexpr std::uint32_t row_limit = 1048576;
  static constexpr std::uint32_t column_limit = 16384;

  /// Moves rows, cells and dependent references and parts (ECMA-376
  /// 18.3.1.73, 18.3.1.4).
  void edit_sheet(const ElementIdentifier element_id,
                  formula::SheetEdit edit) const {
    const bool rows_edited = edit.axis == formula::Axis::row;
    ElementRegistry::Sheet &sheet = m_registry->sheet_element_at(element_id);
    edit.sheet = sheet.name;
    pugi::xml_node sheet_node = get_node(element_id);

    // decided before anything is written
    for (const pugi::xml_node merge :
         sheet_node.child("mergeCells").children("mergeCell")) {
      if (cuts(merge.attribute("ref").value(), edit)) {
        throw UnsupportedOperation();
      }
    }
    std::optional<std::uint32_t> last;
    for (const auto &[position, cell] : sheet.cells) {
      last = std::max(last.value_or(0),
                      rows_edited ? position.row : position.column);
      if (const pugi::xml_node formula = cell.node.child("f");
          std::string_view(formula.attribute("t").value()) == "array" &&
          cuts(formula.attribute("ref").value(), edit)) {
        throw UnsupportedOperation();
      }
    }
    // a `row` may state a format and no cell
    if (rows_edited && !sheet.rows.empty()) {
      last = std::max(last.value_or(0),
                      std::ranges::max(sheet.rows | std::views::keys));
    }
    if (edit.insert && last.has_value() && *last >= edit.index &&
        std::uint64_t{*last} + edit.count >=
            (rows_edited ? row_limit : column_limit)) {
      throw UnsupportedOperation();
    }

    // the parts the edit moves anything in, read before anything is written
    const ElementRegistry::ElementRelations &relations =
        *m_registry->element_relations(element_id);
    const auto related = [&](const pugi::xml_node reference) {
      const char *id = reference.attribute("r:id").value();
      const auto target = relations.relations->find(id);
      return target == relations.relations->end()
                 ? pugi::xml_node()
                 : m_document->part(
                       relations.origin.parent().join(RelPath(target->second)));
    };
    pugi::xml_node drawing;
    pugi::xml_node notes;
    pugi::xml_node comments;
    pugi::xml_node threaded;
    std::vector<pugi::xml_node> tables;
    std::vector<pugi::xml_node> pivots;
    std::vector<pugi::xml_node> caches;
    try {
      pivots = m_document->related_parts(relations.origin, "pivotTable");
      caches = m_document->related_parts(AbsPath("/xl/workbook.xml"),
                                         "pivotCacheDefinition");
      drawing = related(sheet_node.child("drawing"));
      notes = related(sheet_node.child("legacyDrawing"));
      comments = m_document->related_part(relations.origin, "comments");
      threaded = m_document->related_part(relations.origin, "threadedComment");
      for (const pugi::xml_node part :
           sheet_node.child("tableParts").children("tablePart")) {
        if (const pugi::xml_node table = related(part)) {
          tables.push_back(table);
        }
      }
    } catch (const std::exception &) {
      throw UnsupportedOperation(); // a part that does not parse, as VML may
    }
    if (std::ranges::any_of(tables,
                            [&](const pugi::xml_node table) {
                              return cuts_table(table, edit);
                            }) ||
        std::ranges::any_of(pivots,
                            [&](const pugi::xml_node pivot) {
                              return cuts_pivot(pivot, edit);
                            }) ||
        std::ranges::any_of(caches, [&](const pugi::xml_node cache) {
          return loses_pivot_source(cache, edit);
        })) {
      throw UnsupportedOperation();
    }

    // a chart on any sheet may read the edited one
    std::vector<pugi::xml_node> charts;
    std::vector<NamedWorksheet> other_tables;
    try {
      for (ElementIdentifier id =
               element_first_child(m_document->root_element());
           id != null_element_id; id = element_next_sibling(id)) {
        const ElementRegistry::ElementRelations *sheet_relations =
            m_registry->element_relations(id);
        if (id != element_id) {
          for (const pugi::xml_node table :
               m_document->related_parts(sheet_relations->origin, "table")) {
            other_tables.push_back(
                {m_registry->sheet_element_at(id).name, table});
          }
        }
        const auto target = sheet_relations->relations->find(
            get_node(id).child("drawing").attribute("r:id").value());
        if (target == sheet_relations->relations->end()) {
          continue;
        }
        const AbsPath drawing_path =
            sheet_relations->origin.parent().join(RelPath(target->second));
        const pugi::xml_node drawing_root = m_document->part(drawing_path);
        const Relations &drawing_relations =
            m_document->relations_of(drawing_path);
        for (const pugi::xpath_node chart :
             drawing_root.select_nodes("//*[local-name()='chart']")) {
          if (const auto chart_target = drawing_relations.find(
                  chart.node().attribute("r:id").value());
              chart_target != drawing_relations.end()) {
            charts.push_back(m_document->part(
                drawing_path.parent().join(RelPath(chart_target->second))));
          }
        }
      }
    } catch (const std::exception &) {
      throw UnsupportedOperation();
    }

    std::vector<ElementIdentifier> sheet_ids;
    std::vector<NamedWorksheet> worksheets;
    std::string sheet_id;
    pugi::xml_node sheet_entry =
        m_document->workbook().child("sheets").child("sheet");
    for (ElementIdentifier id = element_first_child(m_document->root_element());
         id != null_element_id; id = element_next_sibling(id)) {
      if (id == element_id) {
        sheet_id = sheet_entry.attribute("sheetId").value();
      }
      sheet_ids.push_back(id);
      worksheets.push_back({.name = m_registry->sheet_element_at(id).name,
                            .node = get_node(id)});
      sheet_entry = sheet_entry.next_sibling("sheet");
    }
    move_workbook_references(
        m_document->workbook(), worksheets,
        m_document->related_part(AbsPath("/xl/workbook.xml"), "calcChain"),
        sheet_id, edit);

    for (const auto &[position, cell] : sheet.cells) {
      if (!move_position(position, edit)) {
        m_registry->invalidate(cell.element_id);
      }
    }
    pugi::xml_node sheet_data = sheet_node.child("sheetData");
    for (pugi::xml_node row_node = sheet_data.child("row"); row_node;) {
      const pugi::xml_node next = row_node.next_sibling("row");
      const std::uint32_t index = row_node.attribute("r").as_uint() - 1;
      if (rows_edited) {
        const auto moved = edit.span(index, index);
        if (!moved.has_value()) {
          sheet_data.remove_child(row_node);
          row_node = next;
          continue;
        }
        row_node.attribute("r").set_value(moved->first + 1);
      } else {
        // a hint of the columns the row holds, which the edit makes wrong
        row_node.remove_attribute("spans");
      }
      for (pugi::xml_node cell = row_node.child("c"); cell;) {
        const pugi::xml_node next_cell = cell.next_sibling("c");
        if (const std::optional<TablePosition> moved = move_position(
                TablePosition(cell.attribute("r").value()), edit)) {
          cell.attribute("r").set_value(moved->to_string().c_str());
        } else {
          row_node.remove_child(cell);
        }
        cell = next_cell;
      }
      row_node = next;
    }
    if (!rows_edited) {
      move_columns(sheet_node, sheet, edit);
    }

    move_sheet_ranges(sheet_node, edit);
    move_breaks(sheet_node, edit);
    move_drawing(drawing, edit);
    move_comments(comments, threaded, notes, edit);
    for (const pugi::xml_node chart : charts) {
      move_chart(chart, edit);
    }
    for (const pugi::xml_node pivot : pivots) {
      move_pivot(pivot, edit);
    }
    for (const pugi::xml_node cache : caches) {
      move_pivot_cache(cache, edit);
    }
    for (const NamedWorksheet &table : other_tables) {
      move_table_formulas(table.node, table.name, edit);
    }
    std::vector<TableHeader> headers;
    for (const pugi::xml_node table : tables) {
      std::ranges::move(move_table(table, edit), std::back_inserter(headers));
    }

    if (pugi::xml_node merges = sheet_node.child("mergeCells")) {
      for (pugi::xml_node merge = merges.child("mergeCell"); merge;) {
        const pugi::xml_node next = merge.next_sibling("mergeCell");
        if (const std::optional<std::string> moved =
                move_addresses(merge.attribute("ref").value(), edit, sheet.name,
                               formula::Syntax::ooxml)) {
          if (moved->empty()) {
            merges.remove_child(merge);
          } else {
            merge.attribute("ref").set_value(moved->c_str());
          }
        }
        merge = next;
      }
      if (!merges.child("mergeCell")) {
        sheet_node.remove_child(merges);
      } else if (pugi::xml_attribute merge_count = merges.attribute("count")) {
        merge_count.set_value(static_cast<std::uint32_t>(
            std::ranges::distance(merges.children("mergeCell"))));
      }
    }

    if (pugi::xml_attribute ref =
            sheet_node.child("dimension").attribute("ref");
        ref) {
      if (const std::optional<std::string> moved = move_addresses(
              ref.value(), edit, sheet.name, formula::Syntax::ooxml)) {
        ref.set_value(moved->empty() ? "A1" : moved->c_str());
      }
    }
    std::uint32_t &extent =
        rows_edited ? sheet.dimensions.rows : sheet.dimensions.columns;
    if (edit.index < extent) {
      extent = edit.insert ? extent + edit.count
                           : extent - std::min(edit.count, extent - edit.index);
    }

    if (rows_edited) {
      decltype(sheet.rows) rows;
      for (const auto &[index, entry] : sheet.rows) {
        if (const auto moved = edit.span(index, index)) {
          rows.emplace(moved->first, entry);
        }
      }
      sheet.rows = std::move(rows);
    }
    decltype(sheet.cells) cells;
    for (const auto &[position, cell] : sheet.cells) {
      if (const std::optional<TablePosition> at =
              move_position(position, edit)) {
        m_registry->sheet_cell_element_at(cell.element_id).position = *at;
        cells.emplace(*at, cell);
      }
    }
    sheet.cells = std::move(cells);

    for (const ElementIdentifier id : sheet_ids) {
      index_shared_formulas(m_registry->sheet_element_at(id));
    }
    m_document->drop_sheet_dependencies();
    m_document->note_moved();

    for (const TableHeader &header : headers) {
      sheet_set_cell(element_id, header.position.column, header.position.row,
                     CellValue(header.name));
    }
  }

  /// Moves, splits or shrinks `col` ranges and reindexes them (18.3.1.13).
  /// Inserted columns have no declaration; ranges cannot exceed `XFD`.
  static void move_columns(pugi::xml_node sheet_node,
                           ElementRegistry::Sheet &sheet,
                           const formula::SheetEdit &edit) {
    pugi::xml_node cols = sheet_node.child("cols");
    const auto place = [&](const pugi::xml_node col, const std::uint64_t first,
                           const std::uint64_t last) {
      if (first >= column_limit) {
        cols.remove_child(col);
        return;
      }
      xml::set_attribute(col, "min", std::to_string(first + 1).c_str());
      xml::set_attribute(
          col, "max",
          std::to_string(std::min<std::uint64_t>(last + 1, column_limit))
              .c_str());
    };
    for (pugi::xml_node col = cols.child("col"); col;) {
      const pugi::xml_node next = col.next_sibling("col");
      const std::uint32_t min = col.attribute("min").as_uint() - 1;
      const std::uint32_t max = col.attribute("max").as_uint() - 1;
      if (edit.insert && min < edit.index && edit.index <= max) {
        place(cols.insert_copy_after(col, col),
              std::uint64_t{edit.index} + edit.count,
              std::uint64_t{max} + edit.count);
        place(col, min, edit.index - 1);
      } else if (const auto span = edit.span(min, max)) {
        place(col, span->first, span->second);
      } else {
        cols.remove_child(col);
      }
      col = next;
    }
    if (cols && !cols.child("col")) {
      sheet_node.remove_child(cols);
    }
    register_columns(sheet, sheet_node.child("cols"));
  }

  static void register_columns(ElementRegistry::Sheet &sheet,
                               const pugi::xml_node cols) {
    sheet.columns.clear();
    for (const pugi::xml_node col : cols.children("col")) {
      sheet.register_column(col.attribute("min").as_uint() - 1,
                            col.attribute("max").as_uint() - 1, col);
    }
  }

  /// The masters of the shared groups, as the cells state them now.
  static void index_shared_formulas(ElementRegistry::Sheet &sheet) {
    sheet.shared_formulas.clear();
    for (const auto &[position, cell] : sheet.cells) {
      if (const pugi::xml_node formula = cell.node.child("f");
          std::string_view(formula.attribute("t").value()) == "shared" &&
          !std::string_view(formula.text().get()).empty()) {
        sheet.shared_formulas.emplace(formula.attribute("si").value(),
                                      ElementRegistry::Sheet::SharedFormula{
                                          position, formula.text().get()});
      }
    }
  }

  /// The number format the cell @p cell_id shows its value in.
  [[nodiscard]] const number_format::Format &
  number_format_of(const ElementIdentifier cell_id) const {
    const pugi::xml_node node = get_node(cell_id);
    const pugi::xml_node column_node =
        m_registry->sheet_element_at(element_parent(cell_id))
            .column_node(
                m_registry->sheet_cell_element_at(cell_id).position.column);
    return m_document->style_registry().number_format(
        shown_format(node, node.parent(), column_node).value_or(0));
  }

  /// What the cell shows for @p number: `TRUE` or `FALSE` for a boolean, the
  /// number formatted otherwise.
  [[nodiscard]] std::string shown_value(const ElementIdentifier cell_id,
                                        const pugi::xml_node cell,
                                        const double number) const {
    if (std::string_view(cell.attribute("t").value()) == "b") {
      return number != 0 ? "TRUE" : "FALSE";
    }
    return number_format_of(cell_id).format(number, m_document->epoch());
  }

  /// Resolves an unstated cell style through its row's `customFormat`, then
  /// its column, matching LibreOffice rather than the 18.3.1.4 default of 0.
  static std::optional<std::uint32_t>
  shown_format(const pugi::xml_node cell, const pugi::xml_node row_node,
               const pugi::xml_node column_node) {
    if (const pugi::xml_attribute style = cell.attribute("s")) {
      return style.as_uint();
    }
    if (row_node.attribute("customFormat").as_bool()) {
      return row_node.attribute("s").as_uint();
    }
    if (const pugi::xml_attribute style = column_node.attribute("style")) {
      return style.as_uint();
    }
    return std::nullopt;
  }

  static std::uint32_t shown_format(const pugi::xml_node cell,
                                    const pugi::xml_node column_node) {
    return shown_format(cell, cell.parent(), column_node).value_or(0);
  }

  /// Points the `s` of @p cell at @p base with the delta applied.
  void
  restyle_cell(pugi::xml_node cell, const std::uint32_t base,
               const TableCellStyle &cell_style, const TextStyle &text_style,
               const std::optional<std::uint32_t> number_format_id = {}) const {
    const std::uint32_t format =
        m_document->style_registry().create_cell_format(
            base, cell_style, text_style, number_format_id);
    pugi::xml_attribute attribute = cell.attribute("s");
    if (!attribute) {
      const pugi::xml_attribute reference = cell.attribute("r");
      attribute = reference ? cell.insert_attribute_after("s", reference)
                            : cell.prepend_attribute("s");
    }
    attribute.set_value(format);
  }

  /// Splits or creates a `col` for @p column and reindexes. New declarations
  /// need a width; Excel otherwise treats them as zero-width.
  static pugi::xml_node claim_column(const pugi::xml_node sheet_node,
                                     ElementRegistry::Sheet &sheet,
                                     const std::uint32_t column) {
    static constexpr std::array<std::string_view, 6> order{
        "sheetPr",       "dimension", "sheetViews",
        "sheetFormatPr", "cols",      "sheetData"};
    pugi::xml_node cols = sheet_node.child("cols");
    if (!cols) {
      cols = xml::insert_in_sequence(sheet_node, "cols", order);
    }

    pugi::xml_node node;
    if (const ElementRegistry::Sheet::Column *entry = sheet.column(column);
        entry != nullptr) {
      node = entry->node;
      const std::uint32_t min = node.attribute("min").as_uint();
      const std::uint32_t max = node.attribute("max").as_uint();
      if (column + 1 < max) {
        pugi::xml_node after = cols.insert_copy_after(node, node);
        xml::set_attribute(after, "min", std::to_string(column + 2).c_str());
      }
      if (column + 1 > min) {
        pugi::xml_node before = cols.insert_copy_before(node, node);
        xml::set_attribute(before, "max", std::to_string(column).c_str());
      }
    } else {
      // 18.3.1.17 states the `col`s in column order
      const pugi::xml_node next =
          cols.find_child([column](const pugi::xml_node col) {
            return col.attribute("min").as_uint() > column + 1;
          });
      node = next ? cols.insert_child_before("col", next)
                  : cols.append_child("col");
      const pugi::xml_node format = sheet_node.child("sheetFormatPr");
      const pugi::xml_attribute width = format.attribute("defaultColWidth");
      // Excel's own default for an 11pt Calibri
      xml::set_attribute(node, "width", width ? width.value() : "8.43");
    }
    xml::set_attribute(node, "min", std::to_string(column + 1).c_str());
    xml::set_attribute(node, "max", std::to_string(column + 1).c_str());

    register_columns(sheet, cols);
    return node;
  }

  [[nodiscard]] TableCellStyle
  sheet_cell_style(const ElementIdentifier element_id,
                   const std::uint32_t column,
                   const std::uint32_t row) const override {
    const ElementRegistry::Sheet &sheet_element =
        m_registry->sheet_element_at(element_id);
    const std::optional<std::uint32_t> format = shown_format(
        sheet_element.cell_node(column, row), sheet_element.row_node(row),
        sheet_element.column_node(column));
    if (!format) {
      return {};
    }
    return m_document->style_registry().cell_style(*format).table_cell_style;
  }

  [[nodiscard]] TablePosition
  sheet_cell_position(const ElementIdentifier element_id) const override {
    return m_registry->sheet_cell_element_at(element_id).position;
  }
  [[nodiscard]] bool
  sheet_cell_is_covered(const ElementIdentifier element_id) const override {
    return m_registry->sheet_cell_element_at(element_id).is_covered;
  }
  [[nodiscard]] TableDimensions
  sheet_cell_span(const ElementIdentifier element_id) const override {
    return m_registry->sheet_cell_element_at(element_id).span;
  }
  [[nodiscard]] ValueType
  sheet_cell_value_type(const ElementIdentifier element_id) const override {
    // ECMA-376 `c/@t` defaults to "n" (number); strings come as shared ("s"),
    // inline ("inlineStr"), or formula ("str") cells.
    const pugi::xml_node node = get_node(element_id);
    const std::string type = node.attribute("t").value();
    if (type == "b") {
      return ValueType::boolean;
    }
    if (type == "e") {
      return ValueType::error;
    }
    if (type == "d") {
      return ValueType::date;
    }
    if (type == "s" || type == "str" || type == "inlineStr") {
      return ValueType::string;
    }
    if (node.child("v")) {
      switch (number_format_of(element_id).category()) {
      case number_format::Category::date:
        return ValueType::date;
      case number_format::Category::time:
        return ValueType::time;
      case number_format::Category::number:
        break;
      }
      return ValueType::float_number;
    }
    // a cell stating no value is empty, which one holding an empty text is not
    return ValueType::unknown;
  }
  /// ECMA-376 18.3.1.4 `c`: `v` is the value, `f` the formula, whose
  /// expression a shared group spells on its master only.
  [[nodiscard]] CellValue
  sheet_cell_value(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);

    CellValue result = CellValue(sheet_cell_value_type(element_id));
    if (result.type() == ValueType::float_number ||
        result.type() == ValueType::date || result.type() == ValueType::time ||
        result.type() == ValueType::boolean) {
      if (const std::optional<double> number =
              util::number::parse(node.child("v").text().get())) {
        // a date states days since 1899-12-30, whatever the workbook counts;
        // a time is a duration, which no epoch moves
        result = result
                     .with_number(result.type() == ValueType::date
                                      ? number_format::days_from_serial(
                                            *number, m_document->epoch())
                                      : *number)
                     .with_text(shown_value(element_id, node, *number));
      }
    }
    if (const pugi::xml_node formula = node.child("f")) {
      result = result.with_formula(formula_expression(element_id, formula));
    }
    return result;
  }

  /// [ECMA-376] 18.3.1.40: a member of a shared group states its `si` alone,
  /// and reads the master's expression moved by the offset between the two.
  [[nodiscard]] std::string
  formula_expression(const ElementIdentifier element_id,
                     const pugi::xml_node formula) const {
    std::string text = formula.text().get();
    if (!text.empty() ||
        std::string_view(formula.attribute("t").value()) != "shared") {
      return text;
    }
    const ElementIdentifier sheet_id =
        m_registry->element_at(element_id).parent_id;
    if (sheet_id == null_element_id) {
      return text;
    }
    const ElementRegistry::Sheet &sheet =
        m_registry->sheet_element_at(sheet_id);
    const auto master =
        sheet.shared_formulas.find(formula.attribute("si").value());
    if (master == sheet.shared_formulas.end()) {
      return text;
    }
    std::optional<formula::Node> node =
        formula::parse(master->second.expression, formula::Syntax::ooxml);
    if (!node.has_value()) {
      return master->second.expression;
    }
    const TablePosition position =
        m_registry->sheet_cell_element_at(element_id).position;
    formula::shift(*node,
                   static_cast<std::int64_t>(position.column) -
                       master->second.position.column,
                   static_cast<std::int64_t>(position.row) -
                       master->second.position.row);
    return formula::to_string(*node, formula::Syntax::ooxml);
  }

  [[nodiscard]] TextStyle
  line_break_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] ParagraphStyle
  paragraph_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).paragraph_style;
  }
  [[nodiscard]] TextStyle
  paragraph_text_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] TextStyle
  span_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] std::string
  text_content(const ElementIdentifier element_id) const override {
    const ElementRegistry::Text &text_element =
        m_registry->text_element_at(element_id);

    const pugi::xml_node first = get_node(element_id);
    const pugi::xml_node last = text_element.last;

    // the `v` of a number states its value, and the cell shows it formatted
    if (const pugi::xml_node cell = first.parent();
        std::string_view(first.name()) == "v" &&
        (std::string_view(cell.attribute("t").value()).empty() ||
         std::string_view(cell.attribute("t").value()) == "n" ||
         std::string_view(cell.attribute("t").value()) == "b")) {
      if (const std::optional<double> number =
              util::number::parse(first.text().get())) {
        return shown_value(element_parent(element_id), cell, *number);
      }
    }

    std::string result;
    for (pugi::xml_node node = first; node != last.next_sibling();
         node = node.next_sibling()) {
      result += get_text(node);
    }
    return result;
  }
  [[nodiscard]] TextStyle
  text_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  /// TODO a `<hyperlink>` is not modelled, so a link in a sheet has no href.
  [[nodiscard]] std::string link_href(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }

  [[nodiscard]] AnchorType frame_anchor_type(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return AnchorType::at_page;
  }
  [[nodiscard]] std::optional<Measure>
  frame_x(const ElementIdentifier element_id) const override {
    return read_emus_attribute(get_node(element_id)
                                   .child("xdr:pic")
                                   .child("xdr:spPr")
                                   .child("a:xfrm")
                                   .child("a:off")
                                   .attribute("x"));
  }
  [[nodiscard]] std::optional<Measure>
  frame_y(const ElementIdentifier element_id) const override {
    return read_emus_attribute(get_node(element_id)
                                   .child("xdr:pic")
                                   .child("xdr:spPr")
                                   .child("a:xfrm")
                                   .child("a:off")
                                   .attribute("y"));
  }
  [[nodiscard]] std::optional<Measure>
  frame_width(const ElementIdentifier element_id) const override {
    return read_emus_attribute(get_node(element_id)
                                   .child("xdr:pic")
                                   .child("xdr:spPr")
                                   .child("a:xfrm")
                                   .child("a:ext")
                                   .attribute("cx"));
  }
  [[nodiscard]] std::optional<Measure>
  frame_height(const ElementIdentifier element_id) const override {
    return read_emus_attribute(get_node(element_id)
                                   .child("xdr:pic")
                                   .child("xdr:spPr")
                                   .child("a:xfrm")
                                   .child("a:ext")
                                   .attribute("cy"));
  }
  [[nodiscard]] std::optional<std::int32_t> frame_z_index(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return std::nullopt;
  }
  [[nodiscard]] std::optional<DrawingTransform> frame_transform(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return std::nullopt;
  }
  [[nodiscard]] GraphicStyle frame_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }

  [[nodiscard]] bool
  image_is_internal(const ElementIdentifier element_id) const override {
    try {
      const AbsPath path = Path(image_href(element_id)).make_absolute();
      return m_document->as_filesystem()->is_file(path);
    } catch (...) { // NOLINT(bugprone-empty-catch): any error => not internal
    }
    return false;
  }
  [[nodiscard]] std::optional<File>
  image_file(const ElementIdentifier element_id) const override {
    const AbsPath path = Path(image_href(element_id)).make_absolute();
    return File(m_document->as_filesystem()->open(path));
  }
  [[nodiscard]] std::string
  image_href(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute ref = node.attribute("r:embed"); ref) {
      if (const auto [relations, origin] = get_relations_and_origin(element_id);
          relations != nullptr) {
        if (const auto rel = relations->find(ref.value());
            rel != std::end(*relations)) {
          return origin.parent().join(RelPath(rel->second)).string();
        }
      }
    }
    // an unresolvable relationship leaves no href rather than a broken one
    return "";
  }

private:
  Document *m_document{nullptr};

  [[nodiscard]] pugi::xml_node
  get_node(const ElementIdentifier element_id) const {
    return m_registry->element_at(element_id).node;
  }

  /// A new `row` in @p sheet_data, before the first one past @p row: 18.3.1.80
  /// states them in row order.
  static pugi::xml_node insert_row_node(pugi::xml_node sheet_data,
                                        const std::uint32_t row) {
    for (const pugi::xml_node child : sheet_data.children("row")) {
      if (child.attribute("r").as_uint() > row + 1) {
        return sheet_data.insert_child_before("row", child);
      }
    }
    return sheet_data.append_child("row");
  }

  /// A new `c` in @p row_node, before the first one past @p column: 18.3.1.73
  /// states them in column order.
  static pugi::xml_node insert_cell_node(pugi::xml_node row_node,
                                         const std::uint32_t column) {
    for (const pugi::xml_node child : row_node.children("c")) {
      if (TablePosition(child.attribute("r").value()).column > column) {
        return row_node.insert_child_before("c", child);
      }
    }
    return row_node.append_child("c");
  }

  /// Widens the sheet's extent and its `dimension` (18.3.1.35) to hold
  /// @p position.
  static void grow_dimension(const pugi::xml_node sheet_node,
                             ElementRegistry::Sheet &sheet,
                             const TablePosition &position) {
    if (position.column < sheet.dimensions.columns &&
        position.row < sheet.dimensions.rows) {
      return;
    }
    sheet.dimensions.columns =
        std::max(sheet.dimensions.columns, position.column + 1);
    sheet.dimensions.rows = std::max(sheet.dimensions.rows, position.row + 1);

    pugi::xml_attribute ref = sheet_node.child("dimension").attribute("ref");
    if (!ref) {
      return; // it is optional, and a reader without it takes the cells
    }
    const std::string value = ref.value();
    const TablePosition stated = value.find(':') == std::string::npos
                                     ? TablePosition(value)
                                     : TableRange(value).from();
    const TableRange grown(
        TablePosition(std::min(stated.column, position.column),
                      std::min(stated.row, position.row)),
        TablePosition(sheet.dimensions.columns - 1, sheet.dimensions.rows - 1));
    ref.set_value(grown.to_string().c_str());
  }

  /// Whether a merge covers @p position without anchoring it - Excel ignores
  /// what a covered `c` holds.
  static bool is_covered_by_merge(const pugi::xml_node sheet_node,
                                  const TablePosition &position) {
    for (const pugi::xml_node merge_node :
         sheet_node.child("mergeCells").children("mergeCell")) {
      const std::string ref = merge_node.attribute("ref").value();
      if (ref.find(':') == std::string::npos) {
        continue;
      }
      const TableRange range(ref);
      if (range.contains(position) && !(position == range.from())) {
        return true;
      }
    }
    return false;
  }

  /// The `c` of (@p column, @p row), and the `row` around it, stated where the
  /// file states neither.
  [[nodiscard]] ElementIdentifier insert_cell(const ElementIdentifier sheet_id,
                                              const std::uint32_t column,
                                              const std::uint32_t row) const {
    const pugi::xml_node sheet_node = get_node(sheet_id);
    const TablePosition position(column, row);
    if (is_covered_by_merge(sheet_node, position)) {
      throw UnsupportedOperation();
    }

    ElementRegistry::Sheet &sheet = m_registry->sheet_element_at(sheet_id);

    pugi::xml_node row_node;
    if (const ElementRegistry::Sheet::Row *row_entry = sheet.row(row);
        row_entry != nullptr) {
      row_node = row_entry->node;
    } else {
      pugi::xml_node sheet_data = sheet_node.child("sheetData");
      if (!sheet_data) {
        throw UnsupportedOperation(); // 18.3.1.99 states one for every sheet
      }
      row_node = insert_row_node(sheet_data, row);
      row_node.append_attribute("r").set_value(row + 1);
      sheet.register_row(row, row_node);
    }

    pugi::xml_node cell_node = insert_cell_node(row_node, column);
    cell_node.append_attribute("r").set_value(position.to_string().c_str());

    const auto &[cell_id, unused1, unused2] =
        m_registry->create_sheet_cell_element(cell_node, position);
    m_registry->append_sheet_cell(sheet_id, cell_id);
    sheet.register_cell(column, row, cell_node, cell_id);

    grow_dimension(sheet_node, sheet, position);

    return cell_id;
  }

  [[nodiscard]] std::pair<const Relations *, AbsPath>
  get_relations_and_origin(const ElementIdentifier element_id) const {
    if (element_id == null_element_id) {
      return {nullptr, {}};
    }
    if (const ElementRegistry::ElementRelations *element_relations =
            m_registry->element_relations(element_id);
        element_relations != nullptr) {
      return {element_relations->relations, element_relations->origin};
    }
    const ElementIdentifier parent_id = element_parent(element_id);
    return get_relations_and_origin(parent_id);
  }

  [[nodiscard]] static std::string get_text(const pugi::xml_node node) {
    if (const std::string name = node.name(); name == "t" || name == "v") {
      return node.text().get();
    }

    return "";
  }

  [[nodiscard]] ResolvedStyle
  get_partial_style(const ElementIdentifier element_id) const {
    if (const ElementType type = element_type(element_id);
        type == ElementType::sheet_cell) {
      return get_partial_cell_style(element_id);
    }
    return {};
  }

  [[nodiscard]] ResolvedStyle
  get_partial_cell_style(const ElementIdentifier element_id) const {
    const pugi::xml_node node = get_node(element_id);
    const pugi::xml_node column_node =
        m_registry->sheet_element_at(element_parent(element_id))
            .column_node(
                m_registry->sheet_cell_element_at(element_id).position.column);
    if (const std::optional<std::uint32_t> format =
            shown_format(node, node.parent(), column_node)) {
      return m_document->style_registry().cell_style(*format);
    }
    return {};
  }

  [[nodiscard]] ResolvedStyle
  get_intermediate_style(const ElementIdentifier element_id) const {
    const ElementIdentifier parent_id = element_parent(element_id);
    if (parent_id == null_element_id) {
      return get_partial_style(element_id);
    }
    ResolvedStyle base = get_intermediate_style(parent_id);
    base.override(get_partial_style(element_id));
    return base;
  }
};

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(Document &document, ElementRegistry &registry) {
  return std::make_unique<ElementAdapter>(document, registry);
}

} // namespace

} // namespace odr::internal::ooxml::spreadsheet
