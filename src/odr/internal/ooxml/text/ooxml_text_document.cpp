#include <odr/internal/ooxml/text/ooxml_text_document.hpp>

#include <odr/exceptions.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/abstract/document_element.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/text/ooxml_text_parser.hpp>
#include <odr/internal/util/file_util.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/util/xml_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <cstring>
#include <fstream>
#include <sstream>

namespace odr::internal::ooxml::text {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry);
}

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> files)
    : internal::Document(FileType::office_open_xml_document, DocumentType::text,
                         std::move(files)) {
  m_document_xml = util::xml::parse(*m_files, AbsPath("/word/document.xml"));
  m_styles_xml = util::xml::parse(*m_files, AbsPath("/word/styles.xml"));

  m_document_relations =
      parse_relationships(*m_files, AbsPath("/word/document.xml"));

  m_root_element = parse_tree(
      m_element_registry, m_document_xml.document_element().child("w:body"));

  m_style_registry = StyleRegistry(m_styles_xml.document_element());

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

const Relations &Document::document_relations() const {
  return m_document_relations;
}

bool Document::is_editable() const noexcept { return true; }

bool Document::is_savable(const bool encrypted) const noexcept {
  return !encrypted;
}

void Document::save(const Path &path) const {
  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipArchive archive;

  for (auto walker = m_files->file_walker(AbsPath("/")); !walker->end();
       walker->next()) {
    const AbsPath &abs_path = walker->path();
    RelPath rel_path = walker->path().rebase(AbsPath("/"));
    if (walker->is_directory()) {
      archive.insert_directory(std::end(archive), rel_path);
      continue;
    }
    if (abs_path == AbsPath("/word/document.xml")) {
      // TODO stream
      std::stringstream out;
      m_document_xml.print(out, "", pugi::format_raw);
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
  throw UnsupportedOperation();
}

namespace {

class ElementAdapter final : public abstract::ElementAdapter,
                             public abstract::TextRootAdapter,
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
                             public abstract::ImageAdapter {
public:
  ElementAdapter(const Document &document, ElementRegistry &registry)
      : m_document(&document), m_registry(&registry) {}

  [[nodiscard]] ElementType
  element_type(const ElementIdentifier element_id) const override {
    return m_registry->element(element_id).type;
  }

  [[nodiscard]] ElementIdentifier
  element_parent(const ElementIdentifier element_id) const override {
    return m_registry->element(element_id).parent_id;
  }
  [[nodiscard]] ElementIdentifier
  element_first_child(const ElementIdentifier element_id) const override {
    return m_registry->element(element_id).first_child_id;
  }
  [[nodiscard]] ElementIdentifier
  element_last_child(const ElementIdentifier element_id) const override {
    return m_registry->element(element_id).last_child_id;
  }
  [[nodiscard]] ElementIdentifier
  element_previous_sibling(const ElementIdentifier element_id) const override {
    return m_registry->element(element_id).previous_sibling_id;
  }
  [[nodiscard]] ElementIdentifier
  element_next_sibling(const ElementIdentifier element_id) const override {
    return m_registry->element(element_id).next_sibling_id;
  }

  [[nodiscard]] bool
  element_is_editable(const ElementIdentifier element_id) const override {
    return m_registry->element(element_id).is_editable;
  }

  [[nodiscard]] const TextRootAdapter *
  text_root_adapter(const ElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::root) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const abstract::SlideAdapter *
  slide_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::PageAdapter *
  page_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::SheetAdapter *
  sheet_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::SheetCellAdapter *
  sheet_cell_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::MasterPageAdapter *
  master_page_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const LineBreakAdapter *
  line_break_adapter(const ElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::line_break) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const ParagraphAdapter *
  paragraph_adapter(const ElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::paragraph) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const SpanAdapter *
  span_adapter(const ElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::span) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const TextAdapter *
  text_adapter(const ElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::text) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const LinkAdapter *
  link_adapter(const ElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::link) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const BookmarkAdapter *
  bookmark_adapter(const ElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::bookmark) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const ListItemAdapter *
  list_item_adapter(const ElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::list_item) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const TableAdapter *
  table_adapter(const ElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::table) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const TableColumnAdapter *
  table_column_adapter(const ElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::table_column) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const TableRowAdapter *
  table_row_adapter(const ElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::table_row) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const TableCellAdapter *
  table_cell_adapter(const ElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::table_cell) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const FrameAdapter *
  frame_adapter(const ElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::frame) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const abstract::RectAdapter *
  rect_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::LineAdapter *
  line_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::CircleAdapter *
  circle_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::CustomShapeAdapter *
  custom_shape_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const ImageAdapter *
  image_adapter(const ElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::image) {
      return nullptr;
    }
    return this;
  }

  [[nodiscard]] PageLayout
  text_root_page_layout(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
  }
  [[nodiscard]] ElementIdentifier text_root_first_master_page(
      const ElementIdentifier element_id) const override {
    (void)element_id;
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
    const pugi::xml_node first = get_node(element_id);
    const pugi::xml_node last = m_registry->text_element(element_id).last;

    std::string result;
    for (pugi::xml_node node = first; node != last.next_sibling();
         node = node.next_sibling()) {
      result += get_text(node);
    }
    return result;
  }
  void text_set_content(const ElementIdentifier element_id,
                        const std::string &text) const override {
    const pugi::xml_node first = get_node(element_id);
    const pugi::xml_node last = m_registry->text_element(element_id).last;

    pugi::xml_node parent = first.parent();
    const pugi::xml_node old_first = first;
    const pugi::xml_node old_last = last;
    pugi::xml_node new_first = old_first;
    pugi::xml_node new_last = last;

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
        auto text_node = insert_node("w:t");
        text_node.append_child(pugi::xml_node_type::node_pcdata)
            .text()
            .set(token.string.c_str());
      } break;
      case util::xml::StringToken::Type::spaces: {
        auto text_node = insert_node("w:t");
        text_node.append_attribute("xml:space").set_value("preserve");
        text_node.append_child(pugi::xml_node_type::node_pcdata)
            .text()
            .set(token.string.c_str());
      } break;
      case util::xml::StringToken::Type::tabs: {
        for (std::size_t i = 0; i < token.string.size(); ++i) {
          insert_node("w:tab");
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
  text_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] std::string
  link_href(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute anchor = node.attribute("w:anchor")) {
      return std::string("#") + anchor.value();
    }
    if (const pugi::xml_attribute ref = node.attribute("r:id")) {
      const auto relations = get_document_relations();
      if (const auto rel = relations.find(ref.value());
          rel != std::end(relations)) {
        return rel->second;
      }
    }
    return "";
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
    return m_registry->table_element(element_id).first_column_id;
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
    const pugi::xml_node node = get_node(element_id);
    TableColumnStyle result;
    if (const std::optional<Measure> width =
            read_twips_attribute(node.attribute("w:w"))) {
      result.width = width;
    }
    return result;
  }

  [[nodiscard]] TableRowStyle
  table_row_style(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return m_document->style_registry()
        .partial_table_row_style(node)
        .table_row_style;
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
    const pugi::xml_node node = get_node(element_id);
    return m_document->style_registry()
        .partial_table_cell_style(node)
        .table_cell_style;
  }

  [[nodiscard]] AnchorType
  frame_anchor_type(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);

    if (node.child("wp:inline")) {
      return AnchorType::as_char;
    }
    return AnchorType::as_char; // TODO default?
  }
  [[nodiscard]] std::optional<std::string>
  frame_x(const ElementIdentifier element_id) const override {
    (void)element_id;
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string>
  frame_y(const ElementIdentifier element_id) const override {
    (void)element_id;
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string>
  frame_width(const ElementIdentifier element_id) const override {
    const pugi::xml_node inner_node = get_frame_inner_node(element_id);
    if (const std::optional<Measure> width = read_emus_attribute(
            inner_node.child("wp:extent").attribute("cx"))) {
      return width->to_string();
    }
    return {};
  }
  [[nodiscard]] std::optional<std::string>
  frame_height(const ElementIdentifier element_id) const override {
    const pugi::xml_node inner_node = get_frame_inner_node(element_id);
    if (const std::optional<Measure> height = read_emus_attribute(
            inner_node.child("wp:extent").attribute("cy"))) {
      return height->to_string();
    }
    return {};
  }
  [[nodiscard]] std::optional<std::string>
  frame_z_index(const ElementIdentifier element_id) const override {
    (void)element_id;
    return std::nullopt;
  }
  [[nodiscard]] GraphicStyle
  frame_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
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
    return m_registry->element(element_id).node;
  }

  [[nodiscard]] pugi::xml_node
  get_frame_inner_node(const ElementIdentifier element_id) const {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_node anchor = node.child("wp:anchor")) {
      return anchor;
    }
    if (const pugi::xml_node inline_node = node.child("wp:inline")) {
      return inline_node;
    }
    return {};
  }

  [[nodiscard]] const Relations &get_document_relations() const {
    return m_document->document_relations();
  }

  [[nodiscard]] static std::string get_text(const pugi::xml_node node) {
    const std::string name = node.name();

    if (name == "w:t") {
      return node.text().get();
    }
    if (name == "w:tab") {
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
    if (const char *style_name = get_style_name(element_id)) {
      if (const Style *style = m_document->style_registry().style(style_name)) {
        return style->resolved();
      }
    }
    return {};
  }

  [[nodiscard]] ResolvedStyle
  get_intermediate_style(const ElementIdentifier element_id) const {
    const ElementIdentifier parent_id = element_parent(element_id);
    ResolvedStyle base;
    if (parent_id == null_element_id) {
      base = m_document->style_registry().default_style()->resolved();
    } else {
      base = get_intermediate_style(parent_id);
    }
    base.override(get_partial_style(element_id));
    return base;
  }
};

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry) {
  return std::make_unique<ElementAdapter>(document, registry);
}

} // namespace

} // namespace odr::internal::ooxml::text
