#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/table_position.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/element_adapter.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/table_range.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.hpp>
#include <odr/internal/util/number_util.hpp>
#include <odr/internal/xml/xml_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <algorithm>
#include <array>
#include <ostream>
#include <sstream>
#include <string_view>
#include <utility>

#include <fmt/format.h>

namespace odr::internal::ooxml::spreadsheet {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry);

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
  const auto [styles_xml, _] = parse_xml_(AbsPath("/xl/styles.xml"));

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

  m_style_registry = StyleRegistry(styles_xml.document_element());

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
  pugi::xml_node calc_node =
      calc_pr(m_xml_documents_and_relations.at(AbsPath("/xl/workbook.xml"))
                  .first.document_element());
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
  ElementAdapter(const Document &document, ElementRegistry &registry)
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
  /// ECMA-376 18.3.1.4: a cell states its value as `v`, or as the text under
  /// `is` with `t="inlineStr"`. A written string goes inline - rewriting the
  /// shared entry would rewrite every other cell indexing it.
  void sheet_set_cell(const ElementIdentifier element_id,
                      const std::uint32_t column, const std::uint32_t row,
                      const CellValue &value) const override {
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

    // the elements over the old children keep their ids and stop being
    // reachable
    while (const pugi::xml_node child = node.first_child()) {
      node.remove_child(child);
    }
    node.remove_attribute("t");
    ElementRegistry::Element &cell_element = m_registry->element_at(cell_id);
    cell_element.first_child_id = null_element_id;
    cell_element.last_child_id = null_element_id;

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
    }
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
  [[nodiscard]] TableCellStyle
  sheet_cell_style(const ElementIdentifier element_id,
                   const std::uint32_t column,
                   const std::uint32_t row) const override {
    const ElementRegistry::Sheet &sheet_element =
        m_registry->sheet_element_at(element_id);
    const pugi::xml_node cell_node = sheet_element.cell_node(column, row);

    TableCellStyle result;
    if (const pugi::xml_attribute style_attribute = cell_node.attribute("s")) {
      const ResolvedStyle style =
          m_document->style_registry().cell_style(style_attribute.as_uint());
      result.override(style.table_cell_style);
    }
    return result;
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
    if (type == "s" || type == "str" || type == "inlineStr" || type == "b" ||
        type == "e" || type == "d") {
      return ValueType::string;
    }
    if (node.child("v")) {
      return ValueType::float_number;
    }
    return ValueType::string;
  }
  /// ECMA-376 18.3.1.4 `c`: `v` is the value, `f` the formula, whose
  /// expression a shared group spells on its master only.
  [[nodiscard]] CellValue
  sheet_cell_value(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);

    CellValue result = CellValue(sheet_cell_value_type(element_id));
    if (result.type() == ValueType::float_number) {
      if (const std::optional<double> number =
              util::number::parse(node.child("v").text().get())) {
        result = result.with_number(*number);
      }
    }
    if (const pugi::xml_node formula = node.child("f")) {
      result = result.with_formula(formula.text().get());
    }
    return result;
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

    std::string result;
    for (pugi::xml_node node = first; node != last.next_sibling();
         node = node.next_sibling()) {
      result += get_text(node);
    }
    return result;
  }
  /// A cell's value goes in through `sheet_set_cell`; a run inside one is not
  /// writable. Refusing beats the silent no-op this was: the document declares
  /// `edit`, so a caller has no other way to learn nothing happened.
  void
  text_set_content([[maybe_unused]] const ElementIdentifier element_id,
                   [[maybe_unused]] const std::string &text) const override {
    throw UnsupportedOperation();
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
  const Document *m_document{nullptr};

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
    if (const pugi::xml_attribute style_id = node.attribute("s")) {
      return m_document->style_registry().cell_style(style_id.as_uint());
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
create_element_adapter(const Document &document, ElementRegistry &registry) {
  return std::make_unique<ElementAdapter>(document, registry);
}

} // namespace

} // namespace odr::internal::ooxml::spreadsheet
