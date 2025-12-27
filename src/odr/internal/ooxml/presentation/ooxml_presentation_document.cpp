#include <odr/internal/ooxml/presentation/ooxml_presentation_document.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/abstract/document_element.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/common/style.hpp>
#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_parser.hpp>
#include <odr/internal/util/xml_util.hpp>

#include <cstring>

namespace odr::internal::ooxml::presentation {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry);
}

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> files)
    : internal::Document(FileType::office_open_xml_presentation,
                         DocumentType::presentation, std::move(files)) {
  m_document_xml = util::xml::parse(*m_files, AbsPath("/ppt/presentation.xml"));

  for (const auto &[id, target] :
       parse_relationships(*m_files, AbsPath("/ppt/presentation.xml"))) {
    m_slides_xml[id] =
        util::xml::parse(*m_files, AbsPath("/ppt").join(RelPath(target)));
  }

  m_root_element = parse_tree(m_element_registry,
                              m_document_xml.document_element(), m_slides_xml);

  m_element_adapter = create_element_adapter(*this, m_element_registry);
}

const ElementRegistry &Document::element_registry() const {
  return m_element_registry;
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

namespace {

void resolve_text_style_(const pugi::xml_node node, TextStyle &result) {
  const pugi::xml_node run_properties = node.child("a:rPr");

  if (const pugi::xml_attribute font_name =
          run_properties.child("rFonts").attribute("ascii")) {
    result.font_name = font_name.value();
  }
  if (const std::optional<Measure> font_size =
          read_hundredth_point_attribute(run_properties.attribute("sz"))) {
    result.font_size = font_size;
  }
  if (const std::optional<FontWeight> font_weight =
          read_font_weight_attribute(run_properties.attribute("b"))) {
    result.font_weight = font_weight;
  }
  if (const std::optional<FontStyle> font_style =
          read_font_style_attribute(run_properties.attribute("i"))) {
    result.font_style = font_style;
  }
  if (const bool font_underline =
          read_line_attribute(run_properties.attribute("u"))) {
    result.font_underline = font_underline;
  }
  if (const bool font_line_through =
          read_line_attribute(run_properties.attribute("strike"))) {
    result.font_line_through = font_line_through;
  }
  if (const std::optional<std::string> font_shadow =
          read_shadow_attribute(run_properties.attribute("shadow"))) {
    result.font_shadow = font_shadow;
  }
  if (const std::optional<Color> font_color =
          read_color_attribute(run_properties.attribute("color"))) {
    result.font_color = font_color;
  }
  if (const std::optional<Color> background_color =
          read_color_attribute(run_properties.attribute("highlight"))) {
    result.background_color = background_color;
  }
}

void resolve_paragraph_style_(const pugi::xml_node node,
                              ParagraphStyle &result) {
  const pugi::xml_node paragraph_properties = node.child("a:pPr");

  if (const std::optional<TextAlign> text_align =
          read_text_align_attribute(paragraph_properties.attribute("jc"))) {
    result.text_align = text_align;
  }
  if (const std::optional<Measure> margin_left = read_twips_attribute(
          paragraph_properties.child("ind").attribute("left"))) {
    result.margin.left = margin_left;
  }
  if (const std::optional<Measure> margin_left = read_twips_attribute(
          paragraph_properties.child("ind").attribute("start"))) {
    result.margin.left = margin_left;
  }
  if (const std::optional<Measure> margin_right = read_twips_attribute(
          paragraph_properties.child("ind").attribute("right"))) {
    result.margin.right = margin_right;
  }
  if (const std::optional<Measure> margin_right = read_twips_attribute(
          paragraph_properties.child("ind").attribute("end"))) {
    result.margin.right = margin_right;
  }
}

class ElementAdapter final : public abstract::ElementAdapter,
                             public abstract::SlideAdapter,
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
  [[nodiscard]] const SlideAdapter *
  slide_adapter(const ElementIdentifier element_id) const override {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element == nullptr || element->type != ElementType::slide) {
      return nullptr;
    }
    return this;
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

  [[nodiscard]] PageLayout
  slide_page_layout(const ElementIdentifier element_id) const override {
    (void)element_id;
    // TODO
    return {
        .width = Measure("11.02 in"),
        .height = Measure("8.27 in"),
        .print_orientation = {},
        .margin = {},
    };
  }
  [[nodiscard]] ElementIdentifier
  slide_master_page(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
  }
  [[nodiscard]] std::string
  slide_name(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {}; // TODO
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
        auto text_node = insert_node("a:t");
        text_node.append_child(pugi::xml_node_type::node_pcdata)
            .text()
            .set(token.string.c_str());
      } break;
      case util::xml::StringToken::Type::spaces: {
        auto text_node = insert_node("a:t");
        text_node.append_attribute("xml:space").set_value("preserve");
        text_node.append_child(pugi::xml_node_type::node_pcdata)
            .text()
            .set(token.string.c_str());
      } break;
      case util::xml::StringToken::Type::tabs: {
        for (std::size_t i = 0; i < token.string.size(); ++i) {
          insert_node("a:tab");
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
    (void)element_id;
    return {}; // TODO
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
    (void)element_id;
    return false;
  }
  [[nodiscard]] TableDimensions
  table_cell_span(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {1, 1}; // TODO
  }
  [[nodiscard]] ValueType
  table_cell_value_type(const ElementIdentifier element_id) const override {
    (void)element_id;
    return ValueType::string;
  }
  [[nodiscard]] TableCellStyle
  table_cell_style(const ElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_cell_style;
  }

  [[nodiscard]] AnchorType
  frame_anchor_type(const ElementIdentifier element_id) const override {
    (void)element_id;
    return AnchorType::at_page;
  }
  [[nodiscard]] std::optional<std::string>
  frame_x(const ElementIdentifier element_id) const override {
    if (const std::optional<Measure> x =
            read_emus_attribute(get_node(element_id)
                                    .child("p:spPr")
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
                                    .child("p:spPr")
                                    .child("a:xfrm")
                                    .child("a:off")
                                    .attribute("y"))) {
      return y->to_string();
    }
    return {};
  }
  [[nodiscard]] std::optional<std::string>
  frame_width(const ElementIdentifier element_id) const override {
    if (const std::optional<Measure> cx =
            read_emus_attribute(get_node(element_id)
                                    .child("p:spPr")
                                    .child("a:xfrm")
                                    .child("a:ext")
                                    .attribute("cx"))) {
      return cx->to_string();
    }
    return {};
  }
  [[nodiscard]] std::optional<std::string>
  frame_height(const ElementIdentifier element_id) const override {
    if (const std::optional<Measure> cy =
            read_emus_attribute(get_node(element_id)
                                    .child("p:spPr")
                                    .child("a:xfrm")
                                    .child("a:ext")
                                    .attribute("cy"))) {
      return cy->to_string();
    }
    return {};
  }
  [[nodiscard]] std::optional<std::string>
  frame_z_index(const ElementIdentifier) const override {
    return std::nullopt;
  }
  [[nodiscard]] GraphicStyle
  frame_style(const ElementIdentifier) const override {
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
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element != nullptr) {
      return element->node;
    }
    return {};
  }

  [[nodiscard]] static std::string get_text(const pugi::xml_node node) {
    const std::string name = node.name();

    if (name == "a:t") {
      return node.text().get();
    }
    if (name == "a:tab") {
      return "\t";
    }

    return "";
  }

  [[nodiscard]] ResolvedStyle
  get_partial_style(const ElementIdentifier element_id) const {
    if (const ElementRegistry::Element *element =
            m_registry->element(element_id);
        element != nullptr) {
      if (element->type == ElementType::paragraph) {
        ResolvedStyle result;
        resolve_text_style_(element->node, result.text_style);
        resolve_paragraph_style_(element->node, result.paragraph_style);
        return result;
      }
      if (element->type == ElementType::span) {
        ResolvedStyle result;
        resolve_text_style_(element->node, result.text_style);
        return result;
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
};

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry) {
  return std::make_unique<ElementAdapter>(document, registry);
}

} // namespace

} // namespace odr::internal::ooxml::presentation
