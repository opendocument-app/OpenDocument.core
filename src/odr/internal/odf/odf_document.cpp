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

bool Document::is_editable() const noexcept { return true; }

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
                             public abstract::SheetColumnAdapter,
                             public abstract::SheetRowAdapter,
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
  element_type(const ExtendedElementIdentifier element_id) const override {
    return m_registry->element(element_id).type;
  }

  [[nodiscard]] ExtendedElementIdentifier
  element_parent(const ExtendedElementIdentifier element_id) const override {
    return m_registry->element(element_id).parent_id;
  }
  [[nodiscard]] ExtendedElementIdentifier element_first_child(
      const ExtendedElementIdentifier element_id) const override {
    return ExtendedElementIdentifier(
        m_registry->element(element_id).first_child_id);
  }
  [[nodiscard]] ExtendedElementIdentifier element_last_child(
      const ExtendedElementIdentifier element_id) const override {
    return ExtendedElementIdentifier(
        m_registry->element(element_id).last_child_id);
  }
  [[nodiscard]] ExtendedElementIdentifier element_previous_sibling(
      const ExtendedElementIdentifier element_id) const override {
    return ExtendedElementIdentifier(
        m_registry->element(element_id).previous_sibling_id);
  }
  [[nodiscard]] ExtendedElementIdentifier element_next_sibling(
      const ExtendedElementIdentifier element_id) const override {
    return ExtendedElementIdentifier(
        m_registry->element(element_id).next_sibling_id);
  }

  [[nodiscard]] bool element_is_editable(
      const ExtendedElementIdentifier element_id) const override {
    return m_registry->element(element_id).is_editable;
  }

  [[nodiscard]] const TextRootAdapter *
  text_root_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const SlideAdapter *
  slide_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const PageAdapter *
  page_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const SheetAdapter *
  sheet_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const SheetColumnAdapter *
  sheet_column_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const SheetRowAdapter *
  sheet_row_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const SheetCellAdapter *
  sheet_cell_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const MasterPageAdapter *
  master_page_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const LineBreakAdapter *
  line_break_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const ParagraphAdapter *
  paragraph_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const SpanAdapter *
  span_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const TextAdapter *
  text_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const LinkAdapter *
  link_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const BookmarkAdapter *
  bookmark_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const ListItemAdapter *
  list_item_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const TableAdapter *
  table_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const TableColumnAdapter *
  table_column_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const TableRowAdapter *
  table_row_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const TableCellAdapter *
  table_cell_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const FrameAdapter *
  frame_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const RectAdapter *
  rect_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const LineAdapter *
  line_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const CircleAdapter *
  circle_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const CustomShapeAdapter *
  custom_shape_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }
  [[nodiscard]] const ImageAdapter *
  image_adapter(const ExtendedElementIdentifier) const override {
    return this;
  }

  [[nodiscard]] PageLayout text_root_page_layout(
      const ExtendedElementIdentifier element_id) const override {
    if (const ExtendedElementIdentifier master_page_id =
            text_root_first_master_page(element_id);
        !master_page_id.is_null()) {
      return master_page_page_layout(master_page_id);
    }
    return {};
  }
  [[nodiscard]] ExtendedElementIdentifier text_root_first_master_page(
      const ExtendedElementIdentifier element_id) const override {
    (void)element_id;
    if (const ExtendedElementIdentifier first_master_page_id =
            m_document->style_registry().first_master_page();
        !first_master_page_id.is_null()) {
      return first_master_page_id;
    }
    return {};
  }

  [[nodiscard]] PageLayout
  slide_page_layout(const ExtendedElementIdentifier element_id) const override {
    if (const ExtendedElementIdentifier master_page_id =
            slide_master_page(element_id);
        !master_page_id.is_null()) {
      return master_page_page_layout(master_page_id);
    }
    return {};
  }
  [[nodiscard]] ExtendedElementIdentifier
  slide_master_page(const ExtendedElementIdentifier element_id) const override {
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
  slide_name(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("draw:name").value();
  }

  [[nodiscard]] PageLayout
  page_layout(const ExtendedElementIdentifier element_id) const override {
    if (const ExtendedElementIdentifier master_page_id =
            page_master_page(element_id);
        !master_page_id.is_null()) {
      return master_page_page_layout(master_page_id);
    }
    return {};
  }
  [[nodiscard]] ExtendedElementIdentifier
  page_master_page(const ExtendedElementIdentifier element_id) const override {
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
  page_name(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("draw:name").value();
  }

  [[nodiscard]] std::string
  sheet_name(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("table:name").value();
  }
  [[nodiscard]] TableDimensions
  sheet_dimensions(const ExtendedElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
  }
  [[nodiscard]] TableDimensions
  sheet_content(const ExtendedElementIdentifier element_id,
                const std::optional<TableDimensions> range) const override {
    (void)element_id;
    (void)range;
    return {}; // TODO
  }
  [[nodiscard]] ExtendedElementIdentifier
  sheet_column(const ExtendedElementIdentifier element_id,
               const std::uint32_t column) const override {
    (void)element_id;
    (void)column;
    return {}; // TODO
  }
  [[nodiscard]] ExtendedElementIdentifier
  sheet_row(const ExtendedElementIdentifier element_id,
            const std::uint32_t row) const override {
    (void)element_id;
    (void)row;
    return {}; // TODO
  }
  [[nodiscard]] ExtendedElementIdentifier
  sheet_cell(const ExtendedElementIdentifier element_id,
             const std::uint32_t column,
             const std::uint32_t row) const override {
    (void)element_id;
    (void)column;
    (void)row;
    return {}; // TODO
  }
  [[nodiscard]] ExtendedElementIdentifier
  sheet_first_shape(const ExtendedElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
  }
  [[nodiscard]] TableStyle
  sheet_style(const ExtendedElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
  }

  [[nodiscard]] TableColumnStyle sheet_column_style(
      const ExtendedElementIdentifier element_id) const override {
    const ElementRegistry::Sheet &sheet_registry =
        m_registry->sheet_element(element_id);

    if (const pugi::xml_node column_node =
            sheet_registry.column(element_id.column());
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
    return {};
  }

  [[nodiscard]] TableRowStyle
  sheet_row_style(const ExtendedElementIdentifier element_id) const override {
    const ElementRegistry::Sheet &sheet_registry =
        m_registry->sheet_element(element_id);

    if (const pugi::xml_node column_node = sheet_registry.row(element_id.row());
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
    return {};
  }

  [[nodiscard]] std::uint32_t
  sheet_cell_column(const ExtendedElementIdentifier element_id) const override {
    return element_id.column();
  }
  [[nodiscard]] std::uint32_t
  sheet_cell_row(const ExtendedElementIdentifier element_id) const override {
    return element_id.row();
  }
  [[nodiscard]] bool sheet_cell_is_covered(
      const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return std::strcmp(node.name(), "table:covered-table-cell") == 0;
  }
  [[nodiscard]] TableDimensions
  sheet_cell_span(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return {node.attribute("table:number-rows-spanned").as_uint(1),
            node.attribute("table:number-columns-spanned").as_uint(1)};
  }
  [[nodiscard]] ValueType sheet_cell_value_type(
      const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const char *value_type = node.attribute("office:value-type").value();
        std::strcmp("float", value_type) == 0) {
      return ValueType::float_number;
    }
    return ValueType::string;
  }
  [[nodiscard]] TableCellStyle
  sheet_cell_style(const ExtendedElementIdentifier element_id) const override {
    return get_partial_cell_style(element_id).table_cell_style;
  }

  [[nodiscard]] PageLayout master_page_page_layout(
      const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute =
            node.attribute("style:page-layout-name")) {
      return m_document->style_registry().page_layout(attribute.value());
    }
    return {};
  }

  [[nodiscard]] TextStyle
  line_break_style(const ExtendedElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] ParagraphStyle
  paragraph_style(const ExtendedElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).paragraph_style;
  }
  [[nodiscard]] TextStyle paragraph_text_style(
      const ExtendedElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] TextStyle
  span_style(const ExtendedElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] std::string
  text_content(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node first = get_node(element_id);
    const pugi::xml_node last = m_registry->text_element(element_id).last;

    std::string result;
    for (pugi::xml_node node = first; node != last.next_sibling();
         node = node.next_sibling()) {
      result += get_text(node);
    }
    return result;
  }
  void text_set_content(const ExtendedElementIdentifier element_id,
                        const std::string &text) const override {
    const pugi::xml_node first = get_node(element_id);
    const pugi::xml_node last = m_registry->text_element(element_id).last;

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

    m_registry->element(element_id).node = new_first;
    m_registry->text_element(element_id).last = new_last;

    for (pugi::xml_node node = old_first; node != old_last.next_sibling();) {
      const pugi::xml_node next = node.next_sibling();
      parent.remove_child(node);
      node = next;
    }
  }
  [[nodiscard]] TextStyle
  text_style(const ExtendedElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] std::string
  link_href(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("xlink:href").value();
  }

  [[nodiscard]] std::string
  bookmark_name(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("text:name").value();
  }

  [[nodiscard]] TextStyle
  list_item_style(const ExtendedElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] TableDimensions
  table_dimensions(const ExtendedElementIdentifier element_id) const override {
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
  [[nodiscard]] ExtendedElementIdentifier table_first_column(
      const ExtendedElementIdentifier element_id) const override {
    return ExtendedElementIdentifier(
        m_registry->table_element(element_id).first_column_id);
  }
  [[nodiscard]] ExtendedElementIdentifier
  table_first_row(const ExtendedElementIdentifier element_id) const override {
    return element_first_child(element_id);
  }
  [[nodiscard]] TableStyle
  table_style(const ExtendedElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_style;
  }

  [[nodiscard]] TableColumnStyle table_column_style(
      const ExtendedElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_column_style;
  }

  [[nodiscard]] TableRowStyle
  table_row_style(const ExtendedElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_row_style;
  }

  [[nodiscard]] bool table_cell_is_covered(
      const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return std::strcmp(node.name(), "table:covered-table-cell") == 0;
  }
  [[nodiscard]] TableDimensions
  table_cell_span(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return {node.attribute("table:number-rows-spanned").as_uint(1),
            node.attribute("table:number-columns-spanned").as_uint(1)};
  }
  [[nodiscard]] ValueType table_cell_value_type(
      const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const char *value_type = node.attribute("office:value-type").value();
        std::strcmp("float", value_type) == 0) {
      return ValueType::float_number;
    }
    return ValueType::string;
  }
  [[nodiscard]] TableCellStyle
  table_cell_style(const ExtendedElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_cell_style;
  }

  [[nodiscard]] AnchorType
  frame_anchor_type(const ExtendedElementIdentifier element_id) const override {
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
  frame_x(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:x")) {
      return attribute.value();
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string>
  frame_y(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:y")) {
      return attribute.value();
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string>
  frame_width(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:width")) {
      return attribute.value();
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string>
  frame_height(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:height")) {
      return attribute.value();
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string>
  frame_z_index(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:z-index")) {
      return attribute.value();
    }
    return std::nullopt;
  }
  [[nodiscard]] GraphicStyle
  frame_style(const ExtendedElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] std::string
  rect_x(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:x").value();
  }
  [[nodiscard]] std::string
  rect_y(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:y").value();
  }
  [[nodiscard]] std::string
  rect_width(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:width").value();
  }
  [[nodiscard]] std::string
  rect_height(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:height").value();
  }
  [[nodiscard]] GraphicStyle
  rect_style(const ExtendedElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] std::string
  line_x1(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:x1").value();
  }
  [[nodiscard]] std::string
  line_y1(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:y1").value();
  }
  [[nodiscard]] std::string
  line_x2(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:x2").value();
  }
  [[nodiscard]] std::string
  line_y2(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:y2").value();
  }
  [[nodiscard]] GraphicStyle
  line_style(const ExtendedElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] std::string
  circle_x(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:x").value();
  }
  [[nodiscard]] std::string
  circle_y(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:y").value();
  }
  [[nodiscard]] std::string
  circle_width(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:width").value();
  }
  [[nodiscard]] std::string
  circle_height(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:height").value();
  }
  [[nodiscard]] GraphicStyle
  circle_style(const ExtendedElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] std::optional<std::string>
  custom_shape_x(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:x")) {
      return attribute.value();
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string>
  custom_shape_y(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:y")) {
      return attribute.value();
    }
    return std::nullopt;
  }
  [[nodiscard]] std::string custom_shape_width(
      const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:width").value();
  }
  [[nodiscard]] std::string custom_shape_height(
      const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("svg:height").value();
  }
  [[nodiscard]] GraphicStyle custom_shape_style(
      const ExtendedElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] bool
  image_is_internal(const ExtendedElementIdentifier element_id) const override {
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
  image_file(const ExtendedElementIdentifier element_id) const override {
    if (m_document->as_filesystem() == nullptr) {
      return std::nullopt;
    }
    const AbsPath path = Path(image_href(element_id)).make_absolute();
    return File(m_document->as_filesystem()->open(path));
  }
  [[nodiscard]] std::string
  image_href(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("xlink:href").value();
  }

private:
  const Document *m_document{nullptr};
  ElementRegistry *m_registry{nullptr};

  [[nodiscard]] pugi::xml_node
  get_node(const ExtendedElementIdentifier element_id) const {
    return m_registry->element(element_id).node;
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
  get_style_name(const ExtendedElementIdentifier element_id) const {
    const pugi::xml_node node = get_node(element_id);
    for (pugi::xml_attribute attribute : node.attributes()) {
      if (util::string::ends_with(attribute.name(), ":style-name")) {
        return attribute.value();
      }
    }
    return {};
  }

  [[nodiscard]] ResolvedStyle
  get_partial_style(const ExtendedElementIdentifier element_id) const {
    if (const ElementType type = element_type(element_id);
        type == ElementType::sheet_cell) {
      return get_partial_cell_style(element_id);
    }
    if (const char *style_name = get_style_name(element_id)) {
      if (const Style *style = m_document->style_registry().style(style_name)) {
        return style->resolved();
      }
    }
    return {};
  }

  [[nodiscard]] ResolvedStyle
  get_intermediate_style(const ExtendedElementIdentifier element_id) const {
    const ExtendedElementIdentifier parent_id = element_parent(element_id);
    if (parent_id.is_null()) {
      return get_partial_style(element_id);
    }
    ResolvedStyle base = get_intermediate_style(parent_id);
    base.override(get_partial_style(element_id));
    return base;
  }

  ResolvedStyle
  get_partial_cell_style(const ExtendedElementIdentifier element_id) const {
    const char *style_name = nullptr;

    const ElementRegistry::Sheet sheet_registry =
        m_registry->sheet_element(element_id.without_extra());
    const auto [column, row] = element_id.cell();

    const pugi::xml_node cell_node = sheet_registry.cell(column, row);
    if (const pugi::xml_attribute attr =
            cell_node.attribute("table:style-name")) {
      style_name = attr.value();
    }

    if (style_name == nullptr) {
      const pugi::xml_node row_node = sheet_registry.row(row);
      if (const pugi::xml_attribute attr =
              row_node.attribute("table:default-cell-style-name")) {
        style_name = attr.value();
      }
    }
    if (style_name == nullptr) {
      const pugi::xml_node column_node = sheet_registry.column(column);
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
