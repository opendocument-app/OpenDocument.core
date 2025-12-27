#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/table_position.hpp>

#include <odr/internal/abstract/document_element.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.hpp>
#include <odr/internal/util/xml_util.hpp>

#include <utility>

namespace odr::internal::ooxml::spreadsheet {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry,
                       StyleRegistry &style_registry);
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

  m_element_adapter =
      create_element_adapter(*this, m_element_registry, m_style_registry);
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
                             public abstract::SheetCellAdapter,
                             public abstract::LineBreakAdapter,
                             public abstract::ParagraphAdapter,
                             public abstract::SpanAdapter,
                             public abstract::TextAdapter,
                             public abstract::LinkAdapter,
                             public abstract::FrameAdapter,
                             public abstract::ImageAdapter {
public:
  ElementAdapter(const Document &document, ElementRegistry &registry,
                 StyleRegistry &style_registry)
      : m_document(&document), m_registry(&registry),
        m_style_registry(&style_registry) {}

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
      return element->is_editable;
    }
    return false;
  }

  [[nodiscard]] const abstract::TextRootAdapter *
  text_root_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::SlideAdapter *
  slide_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::PageAdapter *
  page_adapter(const ElementIdentifier) const override {
    return nullptr;
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
  [[nodiscard]] const abstract::MasterPageAdapter *
  master_page_adapter(const ElementIdentifier) const override {
    return nullptr;
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
  [[nodiscard]] const abstract::BookmarkAdapter *
  bookmark_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::ListItemAdapter *
  list_item_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::TableAdapter *
  table_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::TableColumnAdapter *
  table_column_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::TableRowAdapter *
  table_row_adapter(const ElementIdentifier) const override {
    return nullptr;
  }
  [[nodiscard]] const abstract::TableCellAdapter *
  table_cell_adapter(const ElementIdentifier) const override {
    return nullptr;
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
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::image) {
      return nullptr;
    }
    return this;
  }

  [[nodiscard]] std::string
  sheet_name(const ElementIdentifier element_id) const override {
    return get_node(element_id).attribute("name").value();
  }
  [[nodiscard]] TableDimensions
  sheet_dimensions(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Sheet *element =
            m_registry->sheet_element(element_id);
        element != nullptr) {
      return element->dimensions;
    }
    return {};
  }
  [[nodiscard]] TableDimensions
  sheet_content(const ElementIdentifier element_id,
                const std::optional<TableDimensions> range) const override {
    (void)range;
    return sheet_dimensions(element_id); // TODO
  }
  [[nodiscard]] ElementIdentifier
  sheet_cell(const ElementIdentifier element_id, const std::uint32_t column,
             const std::uint32_t row) const override {
    if (const ElementRegistry::Sheet *sheet_element =
            m_registry->sheet_element(element_id);
        sheet_element != nullptr) {
      if (const ElementRegistry::Sheet::Cell *cell =
              sheet_element->cell(column, row);
          cell != nullptr) {
        return cell->element_id;
      }
    }
    return null_element_id;
  }
  [[nodiscard]] ElementIdentifier
  sheet_first_shape(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Sheet *sheet_element =
            m_registry->sheet_element(element_id);
        sheet_element != nullptr) {
      return sheet_element->first_shape_id;
    }
    return null_element_id;
  }
  [[nodiscard]] TableStyle
  sheet_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
  }
  [[nodiscard]] TableColumnStyle
  sheet_column_style(const ElementIdentifier element_id,
                     const std::uint32_t column) const override {
    TableColumnStyle result;
    if (const ElementRegistry::Sheet *sheet_element =
            m_registry->sheet_element(element_id);
        sheet_element != nullptr) {
      const pugi::xml_node column_node = sheet_element->column_node(column);
      if (const pugi::xml_attribute width = column_node.attribute("width")) {
        result.width = Measure(width.as_float(), DynamicUnit("ch"));
      }
    }
    return result;
  }
  [[nodiscard]] TableRowStyle
  sheet_row_style(const ElementIdentifier element_id,
                  const std::uint32_t row) const override {
    TableRowStyle result;
    if (const ElementRegistry::Sheet *sheet_element =
            m_registry->sheet_element(element_id);
        sheet_element != nullptr) {
      const pugi::xml_node row_node = sheet_element->row_node(row);
      if (const pugi::xml_attribute height = row_node.attribute("ht")) {
        result.height = Measure(height.as_float(), DynamicUnit("pt"));
      }
    }
    return result;
  }
  [[nodiscard]] TableCellStyle
  sheet_cell_style(const ElementIdentifier element_id,
                   const std::uint32_t column,
                   const std::uint32_t row) const override {
    TableCellStyle result;
    if (const ElementRegistry::Sheet *sheet_element =
            m_registry->sheet_element(element_id);
        sheet_element != nullptr) {
      if (const pugi::xml_attribute style_attr =
              sheet_element->cell_node(column, row).attribute("s");
          style_attr) {
        const std::uint32_t style_id = style_attr.as_uint();
        const ResolvedStyle style = m_style_registry->cell_style(style_id);
        result.override(style.table_cell_style);
      }
    }
    return result;
  }

  [[nodiscard]] TablePosition
  sheet_cell_position(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::SheetCell *cell_element =
            m_registry->sheet_cell_element(element_id);
        cell_element != nullptr) {
      return cell_element->position;
    }
    return {};
  }
  [[nodiscard]] bool
  sheet_cell_is_covered(const ElementIdentifier element_id) const override {
    (void)element_id;
    return false; // TODO
  }
  [[nodiscard]] TableDimensions
  sheet_cell_span(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {1, 1}; // TODO
  }
  [[nodiscard]] ValueType
  sheet_cell_value_type(const ElementIdentifier element_id) const override {
    (void)element_id;
    return ValueType::string; // TODO
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
    (void)element_id;
    (void)text;
    // TODO
  }
  [[nodiscard]] TextStyle
  text_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] std::string
  link_href(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
  }

  [[nodiscard]] AnchorType
  frame_anchor_type(const ElementIdentifier element_id) const override {
    return AnchorType::at_page;
  }
  [[nodiscard]] std::optional<std::string>
  frame_x(const ElementIdentifier element_id) const override {
    if (const std::optional<Measure> x =
            read_emus_attribute(get_node(element_id)
                                    .child("xdr:pic")
                                    .child("xdr:spPr")
                                    .child("a:xfrm")
                                    .child("a:off")
                                    .attribute("x"))) {
      return x->to_string();
    }
    return {};
  }
  [[nodiscard]] std::optional<std::string>
  frame_y(const ElementIdentifier element_id) const override {
    if (const std::optional<Measure> y =
            read_emus_attribute(get_node(element_id)
                                    .child("xdr:pic")
                                    .child("xdr:spPr")
                                    .child("a:xfrm")
                                    .child("a:off")
                                    .attribute("y"))) {
      return y->to_string();
    }
    return {};
  }
  [[nodiscard]] std::optional<std::string>
  frame_width(const ElementIdentifier element_id) const override {
    if (const std::optional<Measure> width =
            read_emus_attribute(get_node(element_id)
                                    .child("xdr:pic")
                                    .child("xdr:spPr")
                                    .child("a:xfrm")
                                    .child("a:ext")
                                    .attribute("cx"))) {
      return width->to_string();
    }
    return {};
  }
  [[nodiscard]] std::optional<std::string>
  frame_height(const ElementIdentifier element_id) const override {
    if (const std::optional<Measure> height =
            read_emus_attribute(get_node(element_id)
                                    .child("xdr:pic")
                                    .child("xdr:spPr")
                                    .child("a:xfrm")
                                    .child("a:ext")
                                    .attribute("cy"))) {
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
  StyleRegistry *m_style_registry{nullptr};

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
create_element_adapter(const Document &document, ElementRegistry &registry,
                       StyleRegistry &style_registry) {
  return std::make_unique<ElementAdapter>(document, registry, style_registry);
}

} // namespace

} // namespace odr::internal::ooxml::spreadsheet
