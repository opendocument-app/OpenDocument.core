#include <odr/internal/odf/odf_document.hpp>

#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>

#include <odr/internal/abstract/document_element.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/odf/odf_element_registry.hpp>
#include <odr/internal/odf/odf_parser.hpp>
#include <odr/internal/util/file_util.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/util/xml_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <cstring>
#include <fstream>
#include <sstream>

#include <pugixml.hpp>

namespace odr::internal::odf {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry);
}

Document::Document(const FileType file_type, const DocumentType document_type,
                   std::shared_ptr<abstract::ReadableFilesystem> files)
    : internal::Document(file_type, document_type, std::move(files)) {
  m_content_xml = util::xml::parse(*m_files, AbsPath("/content.xml"));

  if (m_files->exists(AbsPath("/styles.xml"))) {
    m_styles_xml = util::xml::parse(*m_files, AbsPath("/styles.xml"));
  }

  m_root_element = parse_tree(
      m_element_registry,
      m_content_xml.document_element().child("office:body").first_child());

  m_style_registry = StyleRegistry(*this, m_content_xml.document_element(),
                                   m_styles_xml.document_element());

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

bool Document::is_editable() const noexcept {
  // TODO fix spreadsheet editability
  return m_document_type == DocumentType::text ||
         m_document_type == DocumentType::presentation ||
         m_document_type == DocumentType::drawing;
}

bool Document::is_savable(const bool encrypted) const noexcept {
  return !encrypted;
}

void Document::save(const Path &path) const {
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
      std::stringstream out;
      m_content_xml.print(out, "", pugi::format_raw);
      auto tmp = std::make_shared<MemoryFile>(out.str());
      archive.insert_file(std::end(archive), rel_path, tmp);
      continue;
    }
    if (abs_path == Path("/META-INF/manifest.xml")) {
      // TODO
      auto manifest =
          util::xml::parse(*m_files, AbsPath("/META-INF/manifest.xml"));

      for (auto &&node : manifest.select_nodes("//manifest:encryption-data")) {
        node.node().parent().remove_child(node.node());
      }

      std::stringstream out;
      manifest.print(out, "", pugi::format_raw);
      auto tmp = std::make_shared<MemoryFile>(out.str());
      archive.insert_file(std::end(archive), rel_path, tmp);

      continue;
    }
    archive.insert_file(std::end(archive), rel_path, m_files->open(abs_path));
  }

  std::ofstream ostream = util::file::create(path.string());
  archive.save(ostream);
}

void Document::save(const Path & /*path*/, const char * /*password*/) const {
  // TODO throw if not savable
  throw UnsupportedOperation();
}

namespace {

class ElementAdapter final : public abstract::ElementAdapter,
                             public abstract::TextRootAdapter,
                             public abstract::SlideAdapter,
                             public abstract::PageAdapter,
                             public abstract::SheetAdapter,
                             public abstract::SheetCellAdapter,
                             public abstract::MasterPageAdapter,
                             public abstract::LineBreakAdapter,
                             public abstract::ParagraphAdapter,
                             public abstract::SpanAdapter,
                             public abstract::TextAdapter,
                             public abstract::LinkAdapter,
                             public abstract::BookmarkAdapter,
                             public abstract::ListItemAdapter,
                             public abstract::TableAdapter,
                             public abstract::TableColumnAdapter,
                             public abstract::TableRowAdapter,
                             public abstract::TableCellAdapter,
                             public abstract::FrameAdapter,
                             public abstract::RectAdapter,
                             public abstract::LineAdapter,
                             public abstract::CircleAdapter,
                             public abstract::CustomShapeAdapter,
                             public abstract::ImageAdapter {
public:
  ElementAdapter(const Document &document, ElementRegistry &registry)
      : m_document(&document), m_registry(&registry) {}

  [[nodiscard]] ElementType
  element_type(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element != nullptr) {
      return element->type;
    }
    return ElementType::none;
  }

  [[nodiscard]] ElementIdentifier
  element_parent(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element != nullptr) {
      return element->parent_id;
    }
    return null_element_id;
  }
  [[nodiscard]] ElementIdentifier
  element_first_child(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element != nullptr) {
      return element->first_child_id;
    }
    return null_element_id;
  }
  [[nodiscard]] ElementIdentifier
  element_last_child(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element != nullptr) {
      return element->last_child_id;
    }
    return null_element_id;
  }
  [[nodiscard]] ElementIdentifier
  element_previous_sibling(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element != nullptr) {
      return element->previous_sibling_id;
    }
    return null_element_id;
  }
  [[nodiscard]] ElementIdentifier
  element_next_sibling(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element != nullptr) {
      return element->next_sibling_id;
    }
    return null_element_id;
  }

  [[nodiscard]] bool
  element_is_editable(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element != nullptr) {
      if (element->parent_id != null_element_id) {
        return element_is_editable(element->parent_id);
      }
      if (element->type == ElementType::sheet_cell) {
        const ElementRegistry::SheetCell *sheet_cell =
            m_registry->sheet_cell_element(element_id);
        return sheet_cell != nullptr && !sheet_cell->is_repeated;
      }
      return true;
    }
    return false;
  }

  [[nodiscard]] const TextRootAdapter *
  text_root_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::root) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const SlideAdapter *
  slide_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::slide) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const PageAdapter *
  page_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::page) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const SheetAdapter *
  sheet_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::sheet) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const SheetCellAdapter *
  sheet_cell_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::sheet_cell) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const MasterPageAdapter *
  master_page_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::page) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const LineBreakAdapter *
  line_break_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::line_break) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const ParagraphAdapter *
  paragraph_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::paragraph) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const SpanAdapter *
  span_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::span) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const TextAdapter *
  text_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::text) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const LinkAdapter *
  link_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::link) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const BookmarkAdapter *
  bookmark_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::bookmark) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const ListItemAdapter *
  list_item_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::list_item) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const TableAdapter *
  table_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::table) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const TableColumnAdapter *
  table_column_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::table_column) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const TableRowAdapter *
  table_row_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::table_row) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const TableCellAdapter *
  table_cell_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::table_cell) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const FrameAdapter *
  frame_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::frame) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const RectAdapter *
  rect_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::rect) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const LineAdapter *
  line_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::line) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const CircleAdapter *
  circle_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::circle) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const CustomShapeAdapter *
  custom_shape_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::custom_shape) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const ImageAdapter *
  image_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::image) {
      return nullptr;
    }
    return this;
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
  [[nodiscard]] ElementIdentifier text_root_first_master_page(
      const ElementIdentifier element_id) const override {
    (void)element_id;
    if (const ElementIdentifier first_master_page_id =
            m_document->style_registry().first_master_page();
        first_master_page_id != null_element_id) {
      return first_master_page_id;
    }
    return {};
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
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("draw:name").value();
  }

  [[nodiscard]] std::string
  sheet_name(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("table:name").value();
  }
  [[nodiscard]] TableDimensions
  sheet_dimensions(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Sheet *sheet_registry =
            m_registry->sheet_element(element_id);
        sheet_registry != nullptr) {
      return sheet_registry->dimensions;
    }
    return {};
  }
  [[nodiscard]] TableDimensions
  sheet_content(const ElementIdentifier element_id,
                const std::optional<TableDimensions> range) const override {
    const pugi::xml_node node = get_node(element_id);

    TableDimensions result;

    TableCursor cursor;
    for (auto row : node.children("table:table-row")) {
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
        if (const std::uint32_t new_cols =
                std::max(result.columns, cursor.column());
            cell.first_child() && range && new_rows < range->rows &&
            new_cols < range->columns) {
          result.rows = new_rows;
          result.columns = new_cols;
        }
      }
    }

    return result;
  }
  [[nodiscard]] ElementIdentifier
  sheet_cell(const ElementIdentifier element_id, const std::uint32_t column,
             const std::uint32_t row) const override {
    if (const ElementRegistry::Sheet *sheet_registry =
            m_registry->sheet_element(element_id);
        sheet_registry != nullptr) {
      if (const ElementRegistry::Sheet::Cell *sheet_cell =
              sheet_registry->cell(column, row);
          sheet_cell != nullptr) {
        return sheet_cell->element_id;
      }
    }
    return null_element_id;
  }
  [[nodiscard]] ElementIdentifier
  sheet_first_shape(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
  }
  [[nodiscard]] TableStyle
  sheet_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
  }
  [[nodiscard]] TableColumnStyle
  sheet_column_style(const ElementIdentifier element_id,
                     const std::uint32_t column) const override {
    if (const ElementRegistry::Sheet *sheet_registry =
            m_registry->sheet_element(element_id);
        sheet_registry != nullptr) {
      if (const pugi::xml_node column_node =
              sheet_registry->column_node(column);
          column_node) {
        if (const pugi::xml_attribute attr =
                column_node.attribute("table:style-name")) {
          if (const Style *style =
                  m_document->style_registry().style(attr.value());
              style != nullptr) {
            return style->resolved().table_column_style;
          }
        }
      }
    }
    return {};
  }
  [[nodiscard]] TableRowStyle
  sheet_row_style(const ElementIdentifier element_id,
                  const std::uint32_t row) const override {
    if (const ElementRegistry::Sheet *sheet_registry =
            m_registry->sheet_element(element_id);
        sheet_registry != nullptr) {
      if (const pugi::xml_node column_node = sheet_registry->row_node(row);
          column_node) {
        if (const pugi::xml_attribute attr =
                column_node.attribute("table:style-name")) {
          if (const Style *style =
                  m_document->style_registry().style(attr.value());
              style != nullptr) {
            return style->resolved().table_row_style;
          }
        }
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
    (void)element_id;
    return {0, 0};
  }
  [[nodiscard]] bool
  sheet_cell_is_covered(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return std::strcmp(node.name(), "table:covered-table-cell") == 0;
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

  [[nodiscard]] PageLayout
  master_page_page_layout(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute =
            node.attribute("style:page-layout-name")) {
      return m_document->style_registry().page_layout(attribute.value());
    }
    return {};
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
    const ElementRegistry::Text *text_element =
        m_registry->text_element(element_id);
    if (text_element == nullptr) {
      return "";
    }

    const pugi::xml_node first = get_node(element_id);
    const pugi::xml_node last = text_element->last;

    std::string result;
    for (pugi::xml_node node = first; node != last.next_sibling();
         node = node.next_sibling()) {
      result += get_text(node);
    }
    return result;
  }
  void text_set_content(const ElementIdentifier element_id,
                        const std::string &text) const override {
    ElementRegistry::Element *element = m_registry->element(element_id);
    ElementRegistry::Text *text_element = m_registry->text_element(element_id);
    if (element == nullptr || text_element == nullptr) {
      return;
    }

    const pugi::xml_node first = get_node(element_id);
    const pugi::xml_node last = text_element->last;

    pugi::xml_node parent = first.parent();
    const pugi::xml_node old_first = first;
    const pugi::xml_node old_last = last;
    pugi::xml_node new_first = old_first;
    pugi::xml_node new_last = last;

    const auto insert_pcdata = [&] {
      const pugi::xml_node new_node = parent.insert_child_before(
          pugi::xml_node_type::node_pcdata, old_first);
      if (new_first == old_first) {
        new_first = new_node;
      }
      new_last = new_node;
      return new_node;
    };
    const auto insert_node = [&](const char *node) {
      const pugi::xml_node new_node =
          parent.insert_child_before(node, old_first);
      if (new_first == old_first) {
        new_first = new_node;
      }
      new_last = new_node;
      return new_node;
    };

    for (const util::xml::StringToken &token : util::xml::tokenize_text(text)) {
      switch (token.type) {
      case util::xml::StringToken::Type::none:
        break;
      case util::xml::StringToken::Type::string: {
        auto text_node = insert_pcdata();
        text_node.text().set(token.string.c_str());
      } break;
      case util::xml::StringToken::Type::spaces: {
        auto space_node = insert_node("text:s");
        space_node.prepend_attribute("text:c").set_value(token.string.size());
      } break;
      case util::xml::StringToken::Type::tabs: {
        for (std::size_t i = 0; i < token.string.size(); ++i) {
          insert_node("text:tab");
        }
      } break;
      }
    }

    element->node = new_first;
    text_element->last = new_last;

    for (pugi::xml_node node = old_first; node != old_last.next_sibling();) {
      const pugi::xml_node next = node.next_sibling();
      parent.remove_child(node);
      node = next;
    }
  }
  [[nodiscard]] TextStyle
  text_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] std::string
  link_href(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("xlink:href").value();
  }

  [[nodiscard]] std::string
  bookmark_name(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("text:name").value();
  }

  [[nodiscard]] TextStyle
  list_item_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] TableDimensions
  table_dimensions(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);

    TableDimensions result;
    TableCursor cursor;

    for (auto column : node.children("table:table-column")) {
      const auto columns_repeated =
          column.attribute("table:number-columns-repeated").as_uint(1);
      cursor.add_column(columns_repeated);
    }

    result.columns = cursor.column();
    cursor = {};

    for (auto row : node.children("table:table-row")) {
      const auto rows_repeated =
          row.attribute("table:number-rows-repeated").as_uint(1);
      cursor.add_row(rows_repeated);
    }

    result.rows = cursor.row();

    return result;
  }
  [[nodiscard]] ElementIdentifier
  table_first_column(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Table *table_registry =
            m_registry->table_element(element_id);
        table_registry != nullptr) {
      return table_registry->first_column_id;
    }
    return null_element_id;
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
    const pugi::xml_node node = get_node(element_id);
    return std::strcmp(node.name(), "table:covered-table-cell") == 0;
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
  [[nodiscard]] std::optional<std::string>
  frame_x(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:x")) {
      return attribute.value();
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string>
  frame_y(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:y")) {
      return attribute.value();
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string>
  frame_width(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:width")) {
      return attribute.value();
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string>
  frame_height(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:height")) {
      return attribute.value();
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string>
  frame_z_index(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:z-index")) {
      return attribute.value();
    }
    return std::nullopt;
  }
  [[nodiscard]] GraphicStyle
  frame_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] std::string
  rect_x(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:x").value();
  }
  [[nodiscard]] std::string
  rect_y(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:y").value();
  }
  [[nodiscard]] std::string
  rect_width(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:width").value();
  }
  [[nodiscard]] std::string
  rect_height(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:height").value();
  }
  [[nodiscard]] GraphicStyle
  rect_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] std::string
  line_x1(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:x1").value();
  }
  [[nodiscard]] std::string
  line_y1(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:y1").value();
  }
  [[nodiscard]] std::string
  line_x2(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:x2").value();
  }
  [[nodiscard]] std::string
  line_y2(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:y2").value();
  }
  [[nodiscard]] GraphicStyle
  line_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] std::string
  circle_x(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:x").value();
  }
  [[nodiscard]] std::string
  circle_y(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:y").value();
  }
  [[nodiscard]] std::string
  circle_width(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:width").value();
  }
  [[nodiscard]] std::string
  circle_height(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:height").value();
  }
  [[nodiscard]] GraphicStyle
  circle_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] std::optional<std::string>
  custom_shape_x(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:x")) {
      return attribute.value();
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string>
  custom_shape_y(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:y")) {
      return attribute.value();
    }
    return std::nullopt;
  }
  [[nodiscard]] std::string
  custom_shape_width(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:width").value();
  }
  [[nodiscard]] std::string
  custom_shape_height(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:height").value();
  }
  [[nodiscard]] GraphicStyle
  custom_shape_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] bool
  image_is_internal(const ElementIdentifier element_id) const override {
    if (m_document->as_filesystem() == nullptr) {
      return false;
    }
    try {
      const AbsPath path = Path(image_href(element_id)).make_absolute();
      return m_document->as_filesystem()->is_file(path);
    } catch (...) {
    }
    return false;
  }
  [[nodiscard]] std::optional<odr::File>
  image_file(const ElementIdentifier element_id) const override {
    if (m_document->as_filesystem() == nullptr) {
      return std::nullopt;
    }
    const AbsPath path = Path(image_href(element_id)).make_absolute();
    return File(m_document->as_filesystem()->open(path));
  }
  [[nodiscard]] std::string
  image_href(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("xlink:href").value();
  }

private:
  const Document *m_document{nullptr};
  ElementRegistry *m_registry{nullptr};

  [[nodiscard]] pugi::xml_node
  get_node(const ElementIdentifier element_id) const {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element != nullptr) {
      return element->node;
    }
    return {};
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
    for (pugi::xml_attribute attribute : node.attributes()) {
      if (util::string::ends_with(attribute.name(), ":style-name")) {
        return attribute.value();
      }
    }
    return {};
  }

  [[nodiscard]] ResolvedStyle
  get_partial_style(const ElementIdentifier element_id) const {
    if (const ElementRegistry::SheetCell *cell_registry =
            m_registry->sheet_cell_element(element_id);
        cell_registry != nullptr) {
      return get_partial_cell_style(element_parent(element_id), element_id,
                                    cell_registry->position);
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

  ResolvedStyle get_partial_cell_style(const ElementIdentifier sheet_id,
                                       const ElementIdentifier cell_id,
                                       const TablePosition &position) const {
    const char *style_name = nullptr;

    if (const pugi::xml_attribute attr =
            get_node(cell_id).attribute("table:style-name")) {
      style_name = attr.value();
    }

    const auto [column, row] = position.pair();
    const ElementRegistry::Sheet *sheet_registry =
        m_registry->sheet_element(sheet_id);
    if (sheet_registry == nullptr) {
      return {};
    }

    if (style_name == nullptr) {
      const pugi::xml_node cell_node = sheet_registry->cell_node(column, row);
      if (const pugi::xml_attribute attr =
              cell_node.attribute("table:style-name")) {
        style_name = attr.value();
      }
    }

    if (style_name == nullptr) {
      const pugi::xml_node row_node = sheet_registry->row_node(row);
      if (const pugi::xml_attribute attr =
              row_node.attribute("table:default-cell-style-name")) {
        style_name = attr.value();
      }
    }
    if (style_name == nullptr) {
      const pugi::xml_node column_node = sheet_registry->column_node(column);
      if (const pugi::xml_attribute attr =
              column_node.attribute("table:default-cell-style-name")) {
        style_name = attr.value();
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
