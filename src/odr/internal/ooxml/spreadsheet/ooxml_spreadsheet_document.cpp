#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/abstract/document_element.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/util/xml_util.hpp>

#include <utility>

namespace odr::internal::ooxml::spreadsheet {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry);
}

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> files)
    : internal::Document(FileType::office_open_xml_workbook,
                         DocumentType::spreadsheet, std::move(files)) {
  const AbsPath workbook_path("/xl/workbook.xml");
  auto [workbook_xml, workbook_relations] = parse_xml_(workbook_path);
  auto [styles_xml, _] = parse_xml_(AbsPath("/xl/styles.xml"));

  for (pugi::xml_node sheet_node :
       workbook_xml.document_element().child("sheets").children("sheet")) {
    const char *id = sheet_node.attribute("r:id").value();
    AbsPath sheet_path =
        workbook_path.parent().join(RelPath(workbook_relations.at(id)));
    auto [sheet_xml, sheet_relationships] = parse_xml_(sheet_path);

    if (auto drawing = sheet_xml.document_element().child("drawing")) {
      AbsPath drawing_path = sheet_path.parent().join(
          RelPath(sheet_relationships.at(drawing.attribute("r:id").value())));
      parse_xml_(drawing_path);
    }
  }

  if (m_files->exists(AbsPath("/xl/sharedStrings.xml"))) {
    for (auto [shared_strings_xml, _] =
             parse_xml_(AbsPath("/xl/sharedStrings.xml"));
         auto shared_string : shared_strings_xml.document_element()) {
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

bool Document::is_editable() const noexcept { return false; }

bool Document::is_savable(const bool /*encrypted*/) const noexcept {
  return false;
}

void Document::save(const Path & /*path*/) const {
  throw UnsupportedOperation();
}

void Document::save(const Path & /*path*/, const char * /*password*/) const {
  throw UnsupportedOperation();
}

std::pair<pugi::xml_document &, Relations &>
Document::parse_xml_(const AbsPath &path) {
  pugi::xml_document document = util::xml::parse(*m_files, path);
  Relations relations = parse_relationships(*m_files, path);

  auto [it, _] = m_xml_documents_and_relations.emplace(
      path, std::make_pair(std::move(document), std::move(relations)));
  return {it->second.first, it->second.second};
}

namespace {

class ElementAdapter final : public abstract::ElementAdapter,
                             public abstract::SheetAdapter,
                             public abstract::SheetColumnAdapter,
                             public abstract::SheetRowAdapter,
                             public abstract::SheetCellAdapter,
                             public abstract::LineBreakAdapter,
                             public abstract::ParagraphAdapter,
                             public abstract::SpanAdapter,
                             public abstract::TextAdapter,
                             public abstract::FrameAdapter,
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

  [[nodiscard]] const abstract::TextRootAdapter *
  text_root_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::SlideAdapter *
  slide_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::PageAdapter *
  page_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const SheetAdapter *
  sheet_adapter(const ExtendedElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::sheet) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const SheetColumnAdapter *sheet_column_adapter(
      const ExtendedElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::sheet_column) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const SheetRowAdapter *
  sheet_row_adapter(const ExtendedElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::sheet_row) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const SheetCellAdapter *sheet_cell_adapter(
      const ExtendedElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::sheet_cell) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const abstract::MasterPageAdapter *
  master_page_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const LineBreakAdapter *line_break_adapter(
      const ExtendedElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::line_break) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const ParagraphAdapter *
  paragraph_adapter(const ExtendedElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::paragraph) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const SpanAdapter *
  span_adapter(const ExtendedElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::span) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const TextAdapter *
  text_adapter(const ExtendedElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::text) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const abstract::LinkAdapter *
  link_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::BookmarkAdapter *
  bookmark_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::ListItemAdapter *
  list_item_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::TableAdapter *
  table_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::TableColumnAdapter *
  table_column_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::TableRowAdapter *
  table_row_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::TableCellAdapter *
  table_cell_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const FrameAdapter *
  frame_adapter(const ExtendedElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::frame) {
      return nullptr;
    }
    return this;
  }
  [[nodiscard]] const abstract::RectAdapter *
  rect_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::LineAdapter *
  line_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::CircleAdapter *
  circle_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::CustomShapeAdapter *
  custom_shape_adapter(const ExtendedElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const ImageAdapter *
  image_adapter(const ExtendedElementIdentifier element_id) const override {
    if (m_registry->element(element_id).type != ElementType::image) {
      return nullptr;
    }
    return this;
  }

  [[nodiscard]] std::string
  sheet_name(const ExtendedElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
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
    (void)element_id;
    return {}; // TODO
  }

  [[nodiscard]] TableRowStyle
  sheet_row_style(const ExtendedElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
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
    (void)element_id;
    return {}; // TODO
  }
  [[nodiscard]] TableDimensions
  sheet_cell_span(const ExtendedElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
  }
  [[nodiscard]] ValueType sheet_cell_value_type(
      const ExtendedElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
  }
  [[nodiscard]] TableCellStyle
  sheet_cell_style(const ExtendedElementIdentifier element_id) const override {
    return get_partial_cell_style(element_id).table_cell_style;
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
  text_style(const ExtendedElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] AnchorType
  frame_anchor_type(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);

    if (node.child("wp:inline")) {
      return AnchorType::as_char;
    }
    return AnchorType::as_char; // TODO default?
  }
  [[nodiscard]] std::optional<std::string>
  frame_x(const ExtendedElementIdentifier element_id) const override {
    (void)element_id;
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string>
  frame_y(const ExtendedElementIdentifier element_id) const override {
    (void)element_id;
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string>
  frame_width(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node inner_node = get_frame_inner_node(element_id);
    if (const std::optional<Measure> width = read_emus_attribute(
            inner_node.child("wp:extent").attribute("cx"))) {
      return width->to_string();
    }
    return {};
  }
  [[nodiscard]] std::optional<std::string>
  frame_height(const ExtendedElementIdentifier element_id) const override {
    const pugi::xml_node inner_node = get_frame_inner_node(element_id);
    if (const std::optional<Measure> height = read_emus_attribute(
            inner_node.child("wp:extent").attribute("cy"))) {
      return height->to_string();
    }
    return {};
  }
  [[nodiscard]] std::optional<std::string>
  frame_z_index(const ExtendedElementIdentifier element_id) const override {
    (void)element_id;
    return std::nullopt;
  }
  [[nodiscard]] GraphicStyle
  frame_style(const ExtendedElementIdentifier element_id) const override {
    (void)element_id;
    return {};
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

  [[nodiscard]] pugi::xml_node
  get_frame_inner_node(const ExtendedElementIdentifier element_id) const {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_node anchor = node.child("wp:anchor")) {
      return anchor;
    }
    if (const pugi::xml_node inline_node = node.child("wp:inline")) {
      return inline_node;
    }
    return {};
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
    return {};
  }

  [[nodiscard]] ResolvedStyle
  get_partial_cell_style(const ExtendedElementIdentifier element_id) const {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute style_id = node.attribute("s")) {
      return m_document->style_registry().cell_style(style_id.as_uint());
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
};

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry) {
  return std::make_unique<ElementAdapter>(document, registry);
}

} // namespace

} // namespace odr::internal::ooxml::spreadsheet
