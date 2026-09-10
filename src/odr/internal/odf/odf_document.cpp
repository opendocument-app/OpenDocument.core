#include <odr/internal/odf/odf_document.hpp>

#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/element_adapter.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/odf/odf_chart.hpp>
#include <odr/internal/odf/odf_element_registry.hpp>
#include <odr/internal/odf/odf_geometry.hpp>
#include <odr/internal/odf/odf_list.hpp>
#include <odr/internal/odf/odf_parser.hpp>
#include <odr/internal/odf/odf_table.hpp>
#include <odr/internal/util/number_util.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/xml/xml_tree_edit.hpp>
#include <odr/internal/xml/xml_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>
#include <ostream>
#include <span>
#include <sstream>
#include <string_view>
#include <unordered_map>

#include <fmt/format.h>

namespace odr::internal::odf {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry);
}

Document::Document(const FileType file_type, const DocumentType document_type,
                   std::shared_ptr<abstract::ReadableFilesystem> files,
                   const EncryptionState encryption_state)
    : internal::Document(file_type, document_type, std::move(files),
                         encryption_state) {
  m_content_xml = xml::parse(*m_files, AbsPath("/content.xml"));

  if (m_files->exists(AbsPath("/styles.xml"))) {
    m_styles_xml = xml::parse(*m_files, AbsPath("/styles.xml"));
  }

  init_(m_content_xml.document_element(), m_styles_xml.document_element());
}

Document::Document(const FileType file_type, const DocumentType document_type,
                   pugi::xml_document flat_xml)
    : internal::Document(file_type, document_type, nullptr),
      m_content_xml{std::move(flat_xml)} {
  // content and styles live under the one root
  init_(m_content_xml.document_element(), m_content_xml.document_element());
}

void Document::init_(const pugi::xml_node content_root,
                     const pugi::xml_node styles_root) {
  m_root_element = parse_tree(m_element_registry,
                              content_root.child("office:body").first_child());

  m_style_registry = StyleRegistry(*this, content_root, styles_root);

  resolve_list_numbering(m_element_registry, m_style_registry, m_root_element);

  m_element_adapter = create_element_adapter(*this, m_element_registry);
}

ElementRegistry &Document::element_registry() { return m_element_registry; }

StyleRegistry &Document::style_registry() { return m_style_registry; }

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

  // no package to rebuild: a flat document is the one tree, and `save` puts
  // back the declaration the parse dropped
  if (m_files == nullptr) {
    m_content_xml.save(out, "", pugi::format_raw);
    return;
  }

  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipArchive archive;

  // `mimetype` has to be the first file and uncompressed
  if (m_files->is_file(AbsPath("/mimetype"))) {
    archive.insert_file(std::end(archive), RelPath("mimetype"),
                        m_files->open(AbsPath("/mimetype")), 0);
  }

  for (auto walker = m_files->file_walker(AbsPath("/")); !walker->end();
       walker->next()) {
    const AbsPath &abs_path = walker->path();
    RelPath rel_path = abs_path.rebase(AbsPath("/"));
    if (abs_path == Path("/mimetype")) {
      continue;
    }
    if (walker->is_directory()) {
      archive.insert_directory(std::end(archive), rel_path);
      continue;
    }
    if (abs_path == Path("/content.xml")) {
      // TODO stream
      std::stringstream content;
      m_content_xml.print(content, "", pugi::format_raw);
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

namespace {

/// Absent or unparsable geometry yields nullopt rather than throwing.
std::optional<Measure> read_measure(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return std::nullopt;
  }
  try {
    return Measure(attribute.value());
  } catch (...) { // NOLINT(bugprone-empty-catch)
    return std::nullopt;
  }
}

/// Same, for attributes the API reports unconditionally.
Measure read_measure_or_zero(const pugi::xml_attribute attribute) {
  return read_measure(attribute).value_or(Measure(0, DynamicUnit()));
}

/// The unit an `svg:d` with no view box is written in (19.180).
Measure hundredth_millimetres(const double value) {
  return Measure(value / 100.0, DynamicUnit("mm"));
}

/// A `draw:connector` states no box of its own, so its path is what places it.
std::optional<DrawingPath> connector_box(const pugi::xml_node node) {
  if (std::strcmp(node.name(), "draw:connector") != 0) {
    return {};
  }
  return read_path(node);
}

using TreeEditor = xml::TreeEditor<ElementRegistry>;
using xml::NodeSpan;

/// Writes @p text as odf text nodes before @p before, or at the end of
/// @p parent where that is null. Empty text still gets a node to anchor to.
NodeSpan write_text_nodes(pugi::xml_node parent, const pugi::xml_node before,
                          const std::string &text) {
  NodeSpan span;

  const auto track = [&](const pugi::xml_node new_node) {
    if (!span.first) {
      span.first = new_node;
    }
    span.last = new_node;
    return new_node;
  };
  const auto insert = [&](const pugi::xml_node_type type) {
    return track(before ? parent.insert_child_before(type, before)
                        : parent.append_child(type));
  };
  const auto insert_named = [&](const char *name) {
    return track(before ? parent.insert_child_before(name, before)
                        : parent.append_child(name));
  };

  for (const xml::StringToken &token : xml::tokenize_text(text)) {
    switch (token.type) {
    case xml::StringToken::Type::none:
      break;
    case xml::StringToken::Type::string: {
      auto text_node = insert(pugi::xml_node_type::node_pcdata);
      text_node.text().set(token.string.c_str());
    } break;
    case xml::StringToken::Type::spaces: {
      auto space_node = insert_named("text:s");
      space_node.prepend_attribute("text:c").set_value(token.string.size());
    } break;
    case xml::StringToken::Type::tabs: {
      for (std::size_t i = 0; i < token.string.size(); ++i) {
        insert_named("text:tab");
      }
    } break;
    }
  }

  if (!span.first) {
    insert(pugi::xml_node_type::node_pcdata);
  }
  return span;
}

using AdapterBase = internal::RegistryElementAdapter<
    ElementRegistry, abstract::TextRootAdapter, abstract::SlideAdapter,
    abstract::PageAdapter, abstract::SheetAdapter, abstract::SheetCellAdapter,
    abstract::MasterPageAdapter, abstract::LineBreakAdapter,
    abstract::ParagraphAdapter, abstract::SpanAdapter, abstract::TextAdapter,
    abstract::LinkAdapter, abstract::BookmarkAdapter, abstract::ListAdapter,
    abstract::ListItemAdapter, abstract::TableAdapter,
    abstract::TableColumnAdapter, abstract::TableRowAdapter,
    abstract::TableCellAdapter, abstract::FrameAdapter, abstract::ImageAdapter>;

class ElementAdapter final : public AdapterBase {
public:
  ElementAdapter(const Document &document, ElementRegistry &registry)
      : AdapterBase(registry), m_document(&document) {}

  [[nodiscard]] bool
  element_is_editable(const ElementIdentifier element_id) const override {
    const ElementRegistry::Element &element =
        m_registry->element_at(element_id);
    if (element.type == ElementType::sheet_cell) {
      return !m_registry->sheet_cell_element_at(element_id).is_repeated;
    }
    if (element.parent_id != null_element_id) {
      return element_is_editable(element.parent_id);
    }
    return true;
  }

  [[nodiscard]] PageLayout
  text_root_page_layout(const ElementIdentifier element_id) const override {
    if (const ElementIdentifier master_page_id =
            text_root_first_master_page(element_id);
        master_page_id != null_element_id) {
      return master_page_page_layout(master_page_id);
    }
    return {};
  }
  static bool is_anchored_frame(const pugi::xml_node node) {
    return std::strcmp(node.name(), "draw:frame") == 0;
  }

  /// Whether laying this out would put a word on the page; a frame is anchored,
  /// not written.
  static bool writes_text(const pugi::xml_node node) {
    for (const pugi::xml_node child : node.children()) {
      if (child.type() == pugi::node_pcdata &&
          !std::string_view(child.value()).empty()) {
        return true;
      }
      if (!is_anchored_frame(child) && writes_text(child)) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] ElementIdentifier text_root_first_master_page(
      const ElementIdentifier element_id) const override {
    // A paragraph may name the master page its page uses (20.283). One page box
    // is all we lay out, so only a name ahead of every written word counts.
    for (const pugi::xml_node child : get_node(element_id).children()) {
      if (is_anchored_frame(child)) {
        continue;
      }
      if (const ElementIdentifier master_page_id =
              m_document->style_registry().master_page_of_style(
                  child.attribute("text:style-name").value());
          master_page_id != null_element_id) {
        return master_page_id;
      }
      if (writes_text(child)) {
        break;
      }
    }
    return m_document->style_registry().first_master_page();
  }

  [[nodiscard]] PageLayout
  slide_page_layout(const ElementIdentifier element_id) const override {
    if (const ElementIdentifier master_page_id = slide_master_page(element_id);
        master_page_id != null_element_id) {
      return master_page_page_layout(master_page_id);
    }
    return {};
  }
  [[nodiscard]] ElementIdentifier
  slide_master_page(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute master_page_name_attr =
            node.attribute("draw:master-page-name");
        master_page_name_attr) {
      return m_document->style_registry().master_page(
          master_page_name_attr.value());
    }
    return {};
  }
  [[nodiscard]] std::string
  slide_name(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("draw:name").value();
  }

  [[nodiscard]] PageLayout
  page_layout(const ElementIdentifier element_id) const override {
    if (const ElementIdentifier master_page_id = page_master_page(element_id);
        master_page_id != null_element_id) {
      return master_page_page_layout(master_page_id);
    }
    return {};
  }
  [[nodiscard]] ElementIdentifier
  page_master_page(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute master_page_name_attr =
            node.attribute("draw:master-page-name");
        master_page_name_attr) {
      return m_document->style_registry().master_page(
          master_page_name_attr.value());
    }
    return {};
  }
  [[nodiscard]] std::string
  page_name(const ElementIdentifier element_id) const override {
    return get_node(element_id).attribute("draw:name").value();
  }

  [[nodiscard]] std::string
  sheet_name(const ElementIdentifier element_id) const override {
    return get_node(element_id).attribute("table:name").value();
  }
  [[nodiscard]] PageLayout
  sheet_page_layout(const ElementIdentifier element_id) const override {
    // The table style names the master page (20.383); a sheet that names none
    // takes the first.
    ElementIdentifier master_page_id =
        m_document->style_registry().master_page_of_style(
            get_node(element_id).attribute("table:style-name").value());
    if (master_page_id == null_element_id) {
      master_page_id = m_document->style_registry().first_master_page();
    }
    if (master_page_id == null_element_id) {
      return {};
    }
    return master_page_page_layout(master_page_id);
  }
  [[nodiscard]] TableDimensions
  sheet_dimensions(const ElementIdentifier element_id) const override {
    return m_registry->sheet_element_at(element_id).dimensions;
  }
  [[nodiscard]] TableDimensions
  sheet_content(const ElementIdentifier element_id,
                const std::optional<TableDimensions> range) const override {
    const pugi::xml_node node = get_node(element_id);

    TableDimensions result;

    TableCursor cursor;
    for_each_table_row(node, [&](const pugi::xml_node row) {
      const auto rows_repeated =
          row.attribute("table:number-rows-repeated").as_uint(1);
      cursor.add_row(rows_repeated);

      for (auto cell : row.children("table:table-cell")) {
        const auto columns_repeated =
            cell.attribute("table:number-columns-repeated").as_uint(1);
        const auto colspan =
            cell.attribute("table:number-columns-spanned").as_uint(1);
        const auto rowspan =
            cell.attribute("table:number-rows-spanned").as_uint(1);
        cursor.add_cell(colspan, rowspan, columns_repeated);

        const std::uint32_t new_rows = cursor.row();
        const std::uint32_t new_cols =
            std::max(result.columns, cursor.column());
        if (cell.first_child() &&
            (!range || (new_rows < range->rows && new_cols < range->columns))) {
          result.rows = new_rows;
          result.columns = new_cols;
        }
      }
    });

    return result;
  }
  [[nodiscard]] ElementIdentifier
  sheet_cell(const ElementIdentifier element_id, const std::uint32_t column,
             const std::uint32_t row) const override {
    return m_registry->sheet_cell_id(element_id, column, row);
  }
  [[nodiscard]] ElementIdentifier
  sheet_first_shape(const ElementIdentifier element_id) const override {
    return m_registry->sheet_element_at(element_id).first_shape_id;
  }
  /// [ODF 1.2] 19.385: the value is an attribute and the `text:p` under the
  /// cell shows it, so both are written or the file contradicts itself.
  void sheet_set_cell(const ElementIdentifier element_id,
                      const std::uint32_t column, const std::uint32_t row,
                      const CellValue &value) const override {
    const ElementRegistry::Sheet::Cell *cell =
        m_registry->sheet_element_at(element_id).cell(column, row);

    ElementIdentifier cell_id = null_element_id;
    if (cell == nullptr) {
      // the sheet stops before the position, so there is nothing to refuse
      cell_id = grow_to_cell(element_id, column, row);
    } else {
      cell_id = cell->element_id;

      // both refusals are decided before the split writes anything
      if (cell->node.attribute("table:formula")) {
        throw UnsupportedOperation(); // its dependants would go stale
      }
      if (cell_id != null_element_id && !holds_plain_paragraph(cell_id)) {
        throw UnsupportedOperation();
      }

      if (cell_id == null_element_id ||
          m_registry->sheet_cell_element_at(cell_id).is_repeated) {
        cell_id = claim_cell(element_id, column, row); // `cell` is stale after
      }
    }

    pugi::xml_node node = get_node(cell_id);
    text_set_content(text_run_of(cell_id),
                     value.has_text() ? value.text() : "");

    static constexpr std::array stated = {
        "office:value-type", "office:value",      "office:boolean-value",
        "office:date-value", "office:time-value", "office:string-value",
        "office:currency",   "calcext:value-type"};
    for (const char *attribute : stated) {
      node.remove_attribute(attribute);
    }

    switch (value.type()) {
    case ValueType::unknown:
      break;
    case ValueType::string:
      node.append_attribute("office:value-type").set_value("string");
      break;
    case ValueType::float_number:
      node.append_attribute("office:value-type").set_value("float");
      node.append_attribute("office:value")
          .set_value(fmt::format("{}", value.number()).c_str());
      break;
    }
  }

  [[nodiscard]] TableStyle
  sheet_style(const ElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_style;
  }
  [[nodiscard]] TableColumnStyle
  sheet_column_style(const ElementIdentifier element_id,
                     const std::uint32_t column) const override {
    const ElementRegistry::Sheet &sheet_registry =
        m_registry->sheet_element_at(element_id);
    const pugi::xml_node column_node = sheet_registry.column_node(column);
    if (const pugi::xml_attribute attr =
            column_node.attribute("table:style-name");
        attr) {
      if (const Style *style = m_document->style_registry().style(attr.value());
          style != nullptr) {
        return style->resolved().table_column_style;
      }
    }
    return {};
  }
  [[nodiscard]] TableRowStyle
  sheet_row_style(const ElementIdentifier element_id,
                  const std::uint32_t row) const override {
    const ElementRegistry::Sheet &sheet_registry =
        m_registry->sheet_element_at(element_id);
    const pugi::xml_node row_node = sheet_registry.row_node(row);
    if (const pugi::xml_attribute attr =
            row_node.attribute("table:style-name")) {
      if (const Style *style = m_document->style_registry().style(attr.value());
          style != nullptr) {
        return style->resolved().table_row_style;
      }
    }
    return {};
  }
  [[nodiscard]] TableCellStyle
  sheet_cell_style(const ElementIdentifier element_id,
                   const std::uint32_t column,
                   const std::uint32_t row) const override {
    const ElementIdentifier cell_id = sheet_cell(element_id, column, row);
    return get_partial_cell_style(element_id, cell_id, {column, row})
        .table_cell_style;
  }

  [[nodiscard]] TablePosition
  sheet_cell_position(const ElementIdentifier element_id) const override {
    if (positional_id::holds(element_id)) {
      return {positional_id::column_of(element_id),
              positional_id::row_of(element_id)};
    }
    return m_registry->sheet_cell_element_at(element_id).position;
  }
  [[nodiscard]] bool
  sheet_cell_is_covered(const ElementIdentifier element_id) const override {
    return std::strcmp(get_node(element_id).name(),
                       "table:covered-table-cell") == 0;
  }
  [[nodiscard]] TableDimensions
  sheet_cell_span(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return {node.attribute("table:number-rows-spanned").as_uint(1),
            node.attribute("table:number-columns-spanned").as_uint(1)};
  }
  [[nodiscard]] ValueType
  sheet_cell_value_type(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const char *value_type = node.attribute("office:value-type").value();
        std::strcmp("float", value_type) == 0) {
      return ValueType::float_number;
    }
    return ValueType::string;
  }
  /// [ODF 1.2] 19.386 `office:value`, 19.642 `table:formula`. A date, a time
  /// and a boolean state their value elsewhere and are read as their text.
  [[nodiscard]] CellValue
  sheet_cell_value(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);

    CellValue result = CellValue(sheet_cell_value_type(element_id));
    if (const std::optional<double> number =
            util::number::parse(node.attribute("office:value").value())) {
      result = result.with_number(*number);
    }
    if (const pugi::xml_attribute formula = node.attribute("table:formula")) {
      result = result.with_formula(formula.value());
    }
    return result;
  }

  [[nodiscard]] PageLayout
  master_page_page_layout(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute =
            node.attribute("style:page-layout-name");
        attribute) {
      return m_document->style_registry().page_layout(attribute.value());
    }
    return {};
  }

  [[nodiscard]] TextStyle
  line_break_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] ElementIdentifier
  paragraph_split(const ElementIdentifier element_id,
                  const ElementIdentifier after_id) const override {
    return TreeEditor(*m_registry).split(element_id, after_id);
  }

  void paragraph_merge_next(const ElementIdentifier element_id) const override {
    const ElementIdentifier next_id = element_next_sibling(element_id);
    if (next_id == null_element_id ||
        element_type(next_id) != ElementType::paragraph) {
      throw std::invalid_argument("no paragraph follows the one to merge into");
    }
    TreeEditor(*m_registry).merge_next(element_id);
  }

  [[nodiscard]] ElementIdentifier
  paragraph_insert_after(const ElementIdentifier element_id) const override {
    return TreeEditor(*m_registry).insert_sibling_after(element_id);
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
    const pugi::xml_node end = text_element.last.next_sibling();

    std::string result;
    for (pugi::xml_node node = first; node != end; node = node.next_sibling()) {
      result += get_text(node);
    }
    return result;
  }
  void text_set_content(const ElementIdentifier element_id,
                        const std::string &text) const override {
    ElementRegistry::Element &element = m_registry->element_at(element_id);
    ElementRegistry::Text &text_element =
        m_registry->text_element_at(element_id);

    const NodeSpan old_span{element.node, text_element.last};
    pugi::xml_node parent = old_span.first.parent();
    const NodeSpan new_span = write_text_nodes(parent, old_span.first, text);

    element.node = new_span.first;
    text_element.last = new_span.last;

    xml::remove_nodes(old_span);
  }

  [[nodiscard]] ElementIdentifier
  text_insert(const ElementIdentifier element_id, const Placement where,
              const std::string &text) const override {
    const ElementRegistry::Text &anchor =
        m_registry->text_element_at(element_id);
    pugi::xml_node parent = get_node(element_id).parent();
    // a run beside this one in the same parent carries the same style
    const pugi::xml_node before = where == Placement::after
                                      ? anchor.last.next_sibling()
                                      : get_node(element_id);

    const NodeSpan span = write_text_nodes(parent, before, text);
    const auto &[new_id, unused_element, unused_text] =
        m_registry->create_text_element(span.first, span.last);
    if (where == Placement::after) {
      m_registry->insert_sibling_after(element_id, new_id);
    } else {
      m_registry->insert_sibling_before(element_id, new_id);
    }
    return new_id;
  }

  [[nodiscard]] ElementIdentifier
  element_append_text(const ElementIdentifier element_id,
                      const std::string &text) const override {
    pugi::xml_node node = get_node(element_id);
    const NodeSpan span = write_text_nodes(node, {}, text);
    const auto &[new_id, unused_element, unused_text] =
        m_registry->create_text_element(span.first, span.last);
    m_registry->append_child(element_id, new_id);
    return new_id;
  }

  void element_remove(const ElementIdentifier element_id) const override {
    TreeEditor(*m_registry).remove(element_id);
  }
  [[nodiscard]] TextStyle
  text_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] std::string
  link_href(const ElementIdentifier element_id) const override {
    return get_node(element_id).attribute("xlink:href").value();
  }

  [[nodiscard]] std::string
  bookmark_name(const ElementIdentifier element_id) const override {
    return get_node(element_id).attribute("text:name").value();
  }

  [[nodiscard]] ListType
  list_type(const ElementIdentifier element_id) const override {
    return m_registry->list_type(element_id);
  }

  [[nodiscard]] TextStyle
  list_item_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] std::string
  list_item_marker(const ElementIdentifier element_id) const override {
    return m_registry->list_marker(element_id).text;
  }

  [[nodiscard]] std::optional<std::uint32_t>
  list_item_number(const ElementIdentifier element_id) const override {
    return m_registry->list_marker(element_id).number;
  }

  [[nodiscard]] TableDimensions
  table_dimensions(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);

    TableDimensions result;
    TableCursor cursor;

    for_each_table_column(node, [&](const pugi::xml_node column) {
      const auto columns_repeated =
          column.attribute("table:number-columns-repeated").as_uint(1);
      cursor.add_column(columns_repeated);
    });

    result.columns = cursor.column();
    cursor = {};

    for_each_table_row(node, [&](const pugi::xml_node row) {
      const auto rows_repeated =
          row.attribute("table:number-rows-repeated").as_uint(1);
      cursor.add_row(rows_repeated);
    });

    result.rows = cursor.row();

    return result;
  }
  [[nodiscard]] ElementIdentifier
  table_first_column(const ElementIdentifier element_id) const override {
    return m_registry->table_element_at(element_id).first_column_id;
  }
  [[nodiscard]] ElementIdentifier
  table_first_row(const ElementIdentifier element_id) const override {
    return element_first_child(element_id);
  }
  [[nodiscard]] TableStyle
  table_style(const ElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_style;
  }

  [[nodiscard]] TableColumnStyle
  table_column_style(const ElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_column_style;
  }

  [[nodiscard]] TableRowStyle
  table_row_style(const ElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_row_style;
  }

  [[nodiscard]] bool
  table_cell_is_covered(const ElementIdentifier element_id) const override {
    return std::strcmp(get_node(element_id).name(),
                       "table:covered-table-cell") == 0;
  }
  [[nodiscard]] TableDimensions
  table_cell_span(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return {node.attribute("table:number-rows-spanned").as_uint(1),
            node.attribute("table:number-columns-spanned").as_uint(1)};
  }
  [[nodiscard]] ValueType
  table_cell_value_type(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const char *value_type = node.attribute("office:value-type").value();
        std::strcmp("float", value_type) == 0) {
      return ValueType::float_number;
    }
    return ValueType::string;
  }
  [[nodiscard]] TableCellStyle
  table_cell_style(const ElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_cell_style;
  }

  [[nodiscard]] ShapeType
  frame_shape_type(const ElementIdentifier element_id) const override {
    return m_registry->shape_type(element_id);
  }
  [[nodiscard]] AnchorType
  frame_anchor_type(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);

    const char *anchor_type = node.attribute("text:anchor-type").value();
    if (std::strcmp("as-char", anchor_type) == 0) {
      return AnchorType::as_char;
    }
    if (std::strcmp("char", anchor_type) == 0) {
      return AnchorType::at_char;
    }
    if (std::strcmp("paragraph", anchor_type) == 0) {
      return AnchorType::at_paragraph;
    }
    if (std::strcmp("page", anchor_type) == 0) {
      return AnchorType::at_page;
    }
    return AnchorType::at_page;
  }
  [[nodiscard]] std::optional<Measure>
  frame_x(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const std::optional<Measure> measure =
            read_measure(node.attribute("svg:x"))) {
      return measure;
    }
    if (const std::optional<DrawingPath> box = connector_box(node)) {
      return hundredth_millimetres(box->x);
    }
    return {};
  }
  [[nodiscard]] std::optional<Measure>
  frame_y(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const std::optional<Measure> measure =
            read_measure(node.attribute("svg:y"))) {
      return measure;
    }
    if (const std::optional<DrawingPath> box = connector_box(node)) {
      return hundredth_millimetres(box->y);
    }
    return {};
  }
  [[nodiscard]] std::optional<Measure>
  frame_width(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const std::optional<Measure> measure =
            read_measure(node.attribute("svg:width"))) {
      return measure;
    }
    if (const std::optional<DrawingPath> box = connector_box(node)) {
      return hundredth_millimetres(box->width);
    }
    return {};
  }
  [[nodiscard]] std::optional<Measure>
  frame_height(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const std::optional<Measure> measure =
            read_measure(node.attribute("svg:height"))) {
      return measure;
    }
    if (const std::optional<DrawingPath> box = connector_box(node)) {
      return hundredth_millimetres(box->height);
    }
    return {};
  }
  [[nodiscard]] std::optional<std::int32_t>
  frame_z_index(const ElementIdentifier element_id) const override {
    const pugi::xml_attribute attribute =
        get_node(element_id).attribute("draw:z-index");
    if (!attribute) {
      return std::nullopt;
    }
    return static_cast<std::int32_t>(attribute.as_int());
  }
  [[nodiscard]] std::optional<DrawingTransform>
  frame_transform(const ElementIdentifier element_id) const override {
    return read_transform(get_node(element_id));
  }
  [[nodiscard]] std::optional<DrawingPath>
  frame_path(const ElementIdentifier element_id) const override {
    if (m_registry->shape_type(element_id) != ShapeType::custom) {
      return {};
    }
    return read_path(get_node(element_id));
  }
  [[nodiscard]] std::optional<DrawingLine>
  frame_line(const ElementIdentifier element_id) const override {
    if (m_registry->shape_type(element_id) != ShapeType::line) {
      return {};
    }
    const pugi::xml_node node = get_node(element_id);
    return DrawingLine{
        .x1 = read_measure_or_zero(node.attribute("svg:x1")),
        .y1 = read_measure_or_zero(node.attribute("svg:y1")),
        .x2 = read_measure_or_zero(node.attribute("svg:x2")),
        .y2 = read_measure_or_zero(node.attribute("svg:y2")),
    };
  }
  [[nodiscard]] GraphicStyle
  frame_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] bool
  image_is_internal(const ElementIdentifier element_id) const override {
    if (image_data(element_id)) {
      return true;
    }
    if (m_document->as_filesystem() == nullptr) {
      return false;
    }
    if (is_object(element_id)) {
      return object_file(element_id).has_value();
    }
    try {
      const AbsPath path = Path(image_href(element_id)).make_absolute();
      return m_document->as_filesystem()->is_file(path);
    } catch (...) { // NOLINT(bugprone-empty-catch): any error => not internal
    }
    return false;
  }
  [[nodiscard]] std::optional<File>
  image_file(const ElementIdentifier element_id) const override {
    if (const pugi::xml_node data = image_data(element_id)) {
      return File(std::make_shared<MemoryFile>(
          crypto::util::base64_decode(data.text().get())));
    }
    if (m_document->as_filesystem() == nullptr) {
      return std::nullopt;
    }
    if (is_object(element_id)) {
      return object_file(element_id);
    }
    const AbsPath path = Path(image_href(element_id)).make_absolute();
    return File(m_document->as_filesystem()->open(path));
  }
  [[nodiscard]] std::string
  image_href(const ElementIdentifier element_id) const override {
    // an embedded image has no path of its own, and the renderer names the
    // resource it writes after this
    if (image_data(element_id)) {
      return "image" + std::to_string(element_id);
    }
    std::string href = get_node(element_id).attribute("xlink:href").value();
    if (is_object(element_id)) {
      return replacement_href(element_id).value_or(href + "/chart.svg");
    }
    return href;
  }

private:
  const Document *m_document{nullptr};
  mutable std::mutex m_charts_mutex;
  mutable std::unordered_map<ElementIdentifier, std::optional<std::string>>
      m_charts;

  [[nodiscard]] pugi::xml_node
  get_node(const ElementIdentifier element_id) const {
    return m_registry->element_at(element_id).node;
  }

  /// The attribute where @p node repeats at all, none where it covers one.
  static void set_repeat(pugi::xml_node node, const char *attribute,
                         const std::uint32_t repeated) {
    node.remove_attribute(attribute);
    if (repeated > 1) {
      node.append_attribute(attribute).set_value(repeated);
    }
  }

  /// Cuts the run [@p begin, @p end) so @p at stands alone, copying the parts
  /// around it. @p node stays as the one at @p at, so its element survives.
  static void split_run(pugi::xml_node node, const char *attribute,
                        const std::uint32_t begin, const std::uint32_t end,
                        const std::uint32_t at) {
    if (end > at + 1) {
      set_repeat(node.parent().insert_copy_after(node, node), attribute,
                 end - at - 1);
    }
    if (at > begin) {
      set_repeat(node.parent().insert_copy_before(node, node), attribute,
                 at - begin);
    }
    set_repeat(node, attribute, 1);
  }

  /// Cuts the row run @p row is one position of, so its node stands for that
  /// row alone, and hands that node back. The caller reindexes.
  static pugi::xml_node split_row_at(const ElementRegistry::Sheet &sheet,
                                     const std::uint32_t row) {
    const ElementRegistry::Sheet::Row *row_entry = sheet.row(row);
    const std::size_t row_index = row_entry - sheet.rows.data();
    const std::uint32_t row_begin =
        row_index == 0 ? 0 : sheet.rows[row_index - 1].end;

    split_run(row_entry->node, "table:number-rows-repeated", row_begin,
              row_entry->end, row);

    return row_entry->node;
  }

  /// Gives (@p column, @p row) an element of its own: cuts the row and the
  /// cell run it is one position of, and states the `text:p` an empty cell
  /// has none of. Reindexes: every pointer read before is stale.
  [[nodiscard]] ElementIdentifier claim_cell(const ElementIdentifier sheet_id,
                                             const std::uint32_t column,
                                             const std::uint32_t row) const {
    const ElementRegistry::Sheet &sheet =
        m_registry->sheet_element_at(sheet_id);

    const std::span<const ElementRegistry::Sheet::Cell> cells =
        sheet.row_cells(*sheet.row(row));
    const ElementRegistry::Sheet::Cell *cell_entry = sheet.cell(column, row);
    const std::size_t cell_index = cell_entry - cells.data();
    const std::uint32_t cell_begin =
        cell_index == 0 ? 0 : cells[cell_index - 1].end;
    pugi::xml_node cell_node = cell_entry->node;

    // the row first: the cell keeps its node, so its own run is unmoved
    split_row_at(sheet, row);
    split_run(cell_node, "table:number-columns-repeated", cell_begin,
              cell_entry->end, column);

    if (!cell_node.first_child()) {
      cell_node.append_child("text:p"); // a node with no content gets none
    }

    reindex_sheet(*m_registry, sheet_id);

    return m_registry->sheet_element_at(sheet_id).cell(column, row)->element_id;
  }

  /// What has to follow a `table:table-row` under a `table:table`, and what
  /// has to follow a `table:table-column` ([ODF 1.2] 9.1.2 orders them).
  static constexpr std::array after_rows{
      std::string_view("table:named-expressions")};
  static constexpr std::array after_columns{
      std::string_view("table:table-header-rows"),
      std::string_view("table:table-rows"),
      std::string_view("table:table-row-group"),
      std::string_view("table:table-row"),
      std::string_view("table:named-expressions")};

  /// A new @p name child of @p table, before the first one that has to follow
  /// it.
  static pugi::xml_node
  insert_ordered(pugi::xml_node table, const char *name,
                 const std::span<const std::string_view> after) {
    for (const pugi::xml_node child : table.children()) {
      if (std::ranges::find(after, std::string_view(child.name())) !=
          std::end(after)) {
        return table.insert_child_before(name, child);
      }
    }
    return table.append_child(name);
  }

  static void append_empty_cells(pugi::xml_node row,
                                 const std::uint32_t repeated) {
    set_repeat(row.append_child("table:table-cell"),
               "table:number-columns-repeated", repeated);
  }

  /// Declares the columns the sheet stops before, so its extent reaches
  /// @p column ([ODF 1.2] 9.1.6).
  static void grow_columns(pugi::xml_node sheet_node,
                           ElementRegistry::Sheet &sheet,
                           const std::uint32_t column) {
    if (column < sheet.dimensions.columns) {
      return;
    }
    const std::uint32_t repeated = column + 1 - sheet.dimensions.columns;

    // every declaration comes before the rows, so this lands after all of them
    pugi::xml_node node =
        insert_ordered(sheet_node, "table:table-column", after_columns);
    set_repeat(node, "table:number-columns-repeated", repeated);

    sheet.register_column(sheet.dimensions.columns, repeated, node);
    sheet.dimensions.columns = column + 1;
  }

  /// States the rows and the cells the sheet stops before, so (@p column,
  /// @p row) is a cell of its own holding the `text:p` a value needs.
  /// Reindexes: every pointer read before is stale.
  [[nodiscard]] ElementIdentifier grow_to_cell(const ElementIdentifier sheet_id,
                                               const std::uint32_t column,
                                               const std::uint32_t row) const {
    ElementRegistry::Sheet &sheet = m_registry->sheet_element_at(sheet_id);
    pugi::xml_node sheet_node = get_node(sheet_id);

    pugi::xml_node row_node;
    std::uint32_t cells_end = 0;

    if (const ElementRegistry::Sheet::Row *row_entry = sheet.row(row);
        row_entry == nullptr) {
      const std::uint32_t rows_end =
          sheet.rows.empty() ? 0 : sheet.rows.back().end;
      if (row > rows_end) {
        // a row states at least one cell, so the filler holds an empty one
        pugi::xml_node filler =
            insert_ordered(sheet_node, "table:table-row", after_rows);
        set_repeat(filler, "table:number-rows-repeated", row - rows_end);
        append_empty_cells(filler, sheet.dimensions.columns);
      }
      row_node = insert_ordered(sheet_node, "table:table-row", after_rows);
    } else {
      const std::span<const ElementRegistry::Sheet::Cell> cells =
          sheet.row_cells(*row_entry);
      cells_end = cells.empty() ? 0 : cells.back().end;
      // its cells stand for every position the row repeats over
      row_node = split_row_at(sheet, row);
    }

    if (column > cells_end) {
      append_empty_cells(row_node, column - cells_end);
    }
    row_node.append_child("table:table-cell").append_child("text:p");

    grow_columns(sheet_node, sheet, column);
    reindex_sheet(*m_registry, sheet_id);

    const ElementRegistry::Sheet::Cell *cell =
        m_registry->sheet_element_at(sheet_id).cell(column, row);
    if (cell == nullptr || cell->element_id == null_element_id) {
      // a rowspan out of an earlier row can push the cell off the position;
      // what was appended is empty, so no reader sees a difference
      throw UnsupportedOperation();
    }
    return cell->element_id;
  }

  /// The only child of @p element_id, null where it has none or several.
  [[nodiscard]] ElementIdentifier
  only_child(const ElementIdentifier element_id) const {
    const ElementIdentifier child_id = element_first_child(element_id);
    return child_id != null_element_id &&
                   element_next_sibling(child_id) == null_element_id
               ? child_id
               : null_element_id;
  }

  /// Text, and spans of text, and nothing else, all the way down.
  [[nodiscard]] bool holds_plain_runs(const ElementIdentifier parent_id) const {
    for (ElementIdentifier child_id = element_first_child(parent_id);
         child_id != null_element_id;
         child_id = element_next_sibling(child_id)) {
      const ElementType type = element_type(child_id);
      if (type == ElementType::text) {
        continue;
      }
      if (type != ElementType::span || !holds_plain_runs(child_id)) {
        return false;
      }
    }
    return true;
  }

  /// Whether a write can go through the cell: one paragraph, of text and spans
  /// alone. A link, a line break or a second paragraph is content the write
  /// would take away without the user seeing it go.
  [[nodiscard]] bool
  holds_plain_paragraph(const ElementIdentifier cell_id) const {
    const ElementIdentifier paragraph_id = element_first_child(cell_id);
    if (paragraph_id == null_element_id) {
      return true; // a spanned cell states no paragraph; the write states one
    }
    if (element_next_sibling(paragraph_id) != null_element_id ||
        element_type(paragraph_id) != ElementType::paragraph) {
      return false;
    }
    return holds_plain_runs(paragraph_id);
  }

  /// The run a write goes through: the one the cell holds, so it keeps its
  /// style, and a fresh one where the cell holds none or several. The
  /// paragraph too where the cell states none. @ref holds_plain_paragraph has
  /// to pass.
  [[nodiscard]] ElementIdentifier
  text_run_of(const ElementIdentifier cell_id) const {
    ElementIdentifier paragraph_id = element_first_child(cell_id);
    if (paragraph_id == null_element_id) {
      pugi::xml_node cell_node = get_node(cell_id);
      const auto &[new_id, unused] = m_registry->create_element(
          ElementType::paragraph, cell_node.append_child("text:p"));
      m_registry->append_child(cell_id, new_id);
      paragraph_id = new_id;
    }

    // the deepest element holding the whole content: writing through it keeps
    // the style it carries, as `spreadsheet.js::runOf` does on the page
    ElementIdentifier holder_id = paragraph_id;
    for (ElementIdentifier only_id = only_child(holder_id);
         only_id != null_element_id &&
         element_type(only_id) == ElementType::span;
         only_id = only_child(holder_id)) {
      holder_id = only_id;
    }

    if (const ElementIdentifier text_id = only_child(holder_id);
        text_id != null_element_id &&
        element_type(text_id) == ElementType::text) {
      return text_id;
    }

    // several runs: the elements over the old children keep their ids and stop
    // being reachable
    pugi::xml_node holder_node = get_node(holder_id);
    while (const pugi::xml_node child = holder_node.first_child()) {
      holder_node.remove_child(child);
    }
    ElementRegistry::Element &holder = m_registry->element_at(holder_id);
    holder.first_child_id = null_element_id;
    holder.last_child_id = null_element_id;

    const pugi::xml_node text_node =
        holder_node.append_child(pugi::xml_node_type::node_pcdata);
    const auto &[new_id, unused1, unused2] =
        m_registry->create_text_element(text_node, text_node);
    m_registry->append_child(holder_id, new_id);
    return new_id;
  }

  /// The image's base64 bytes where the markup carries them itself.
  [[nodiscard]] pugi::xml_node
  image_data(const ElementIdentifier element_id) const {
    const pugi::xml_node data =
        get_node(element_id).child("office:binary-data");
    return data.text().empty() ? pugi::xml_node() : data;
  }

  [[nodiscard]] bool is_object(const ElementIdentifier element_id) const {
    return std::strcmp(get_node(element_id).name(), "draw:object") == 0;
  }

  /// The `draw:image` the producer wrote beside the object (10.4.6.2), which
  /// is what draws where the object itself cannot be read.
  [[nodiscard]] std::optional<std::string>
  replacement_href(const ElementIdentifier element_id) const {
    if (chart_svg(element_id).has_value()) {
      return {};
    }
    const pugi::xml_attribute href = get_node(element_id)
                                         .parent()
                                         .child("draw:image")
                                         .attribute("xlink:href");
    if (!href) {
      return {};
    }
    return href.value();
  }

  [[nodiscard]] std::optional<File>
  object_file(const ElementIdentifier element_id) const {
    if (const std::optional<std::string> svg = chart_svg(element_id)) {
      return File(std::make_shared<MemoryFile>(*svg));
    }
    const std::optional<std::string> replacement = replacement_href(element_id);
    if (!replacement.has_value()) {
      return std::nullopt;
    }
    try {
      const AbsPath path = Path(*replacement).make_absolute();
      return File(m_document->as_filesystem()->open(path));
    } catch (...) { // NOLINT(bugprone-empty-catch): no replacement either
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<AbsPath>
  object_part(const ElementIdentifier element_id) const {
    const char *href = get_node(element_id).attribute("xlink:href").value();
    if (href[0] == '\0' || m_document->as_filesystem() == nullptr) {
      return {};
    }
    try {
      AbsPath path = Path(href).make_absolute().join(RelPath("content.xml"));
      if (m_document->as_filesystem()->is_file(path)) {
        return path;
      }
    } catch (...) { // NOLINT(bugprone-empty-catch): no part of its own
    }
    return {};
  }

  /// The object's own part rendered, or nothing where it holds no chart. Kept,
  /// because `image_is_internal`, the resource and the `src` each ask for it.
  [[nodiscard]] const std::optional<std::string> &
  chart_svg(const ElementIdentifier element_id) const {
    const std::lock_guard lock(m_charts_mutex);
    if (const auto it = m_charts.find(element_id); it != m_charts.end()) {
      return it->second;
    }
    std::optional<std::string> result;
    if (const std::optional<AbsPath> path = object_part(element_id)) {
      try {
        const pugi::xml_document content =
            xml::parse(*m_document->as_filesystem()->open(*path));
        result = render_chart(content.document_element());
      } catch (...) { // NOLINT(bugprone-empty-catch): no chart we can read
      }
    }
    return m_charts.emplace(element_id, std::move(result)).first->second;
  }

  [[nodiscard]] static std::string get_text(const pugi::xml_node node) {
    if (node.type() == pugi::node_pcdata) {
      return node.value();
    }

    const std::string name = node.name();
    if (name == "text:s") {
      const std::size_t count = node.attribute("text:c").as_uint(1);
      return std::string(count, ' ');
    }
    if (name == "text:tab") {
      return "\t";
    }
    return "";
  }

  [[nodiscard]] const char *
  get_style_name(const ElementIdentifier element_id) const {
    const pugi::xml_node node = get_node(element_id);
    for (const pugi::xml_attribute attribute : node.attributes()) {
      if (util::string::ends_with(attribute.name(), ":style-name")) {
        return attribute.value();
      }
    }
    return {};
  }

  [[nodiscard]] ResolvedStyle
  get_partial_style(const ElementIdentifier element_id) const {
    if (m_registry->sheet_cell_element(element_id) != nullptr) {
      // the id's position, not the anchor's: the column default is per column
      return get_partial_cell_style(element_parent(element_id), element_id,
                                    sheet_cell_position(element_id));
    }
    if (const char *style_name = get_style_name(element_id);
        style_name != nullptr) {
      if (const Style *style = m_document->style_registry().style(style_name)) {
        return style->resolved();
      }
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

  [[nodiscard]] ResolvedStyle
  get_partial_cell_style(const ElementIdentifier sheet_id,
                         const ElementIdentifier cell_id,
                         const TablePosition &position) const {
    const char *style_name = nullptr;

    if (cell_id != null_element_id) {
      if (const pugi::xml_attribute attribute =
              get_node(cell_id).attribute("table:style-name");
          attribute) {
        style_name = attribute.value();
      }
    }

    const auto [column, row] = position;
    const ElementRegistry::Sheet &sheet_registry =
        m_registry->sheet_element_at(sheet_id);

    if (style_name == nullptr) {
      const pugi::xml_node cell_node = sheet_registry.cell_node(column, row);
      if (const pugi::xml_attribute attribute =
              cell_node.attribute("table:style-name");
          attribute) {
        style_name = attribute.value();
      }
    }

    if (style_name == nullptr) {
      const pugi::xml_node row_node = sheet_registry.row_node(row);
      if (const pugi::xml_attribute attribute =
              row_node.attribute("table:default-cell-style-name");
          attribute) {
        style_name = attribute.value();
      }
    }
    if (style_name == nullptr) {
      const pugi::xml_node column_node = sheet_registry.column_node(column);
      if (const pugi::xml_attribute attribute =
              column_node.attribute("table:default-cell-style-name");
          attribute) {
        style_name = attribute.value();
      }
    }

    if (style_name != nullptr) {
      if (const Style *style = m_document->style_registry().style(style_name);
          style != nullptr) {
        return style->resolved();
      }
    }

    return {};
  }
};

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry) {
  return std::make_unique<ElementAdapter>(document, registry);
}

} // namespace

} // namespace odr::internal::odf
