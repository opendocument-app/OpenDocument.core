#include <odr/internal/ooxml/presentation/ooxml_presentation_document.hpp>

#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/common/style.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_parser.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_style.hpp>
#include <odr/internal/util/document_util.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <iterator>

namespace odr::internal::ooxml::presentation {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry);

} // namespace

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> files)
    : internal::Document(FileType::office_open_xml_presentation,
                         DocumentType::presentation, std::move(files)) {
  m_document_xml = xml::parse(*m_files, AbsPath("/ppt/presentation.xml"));

  // Only the parts the slide-id list names: a package may relate anything at
  // all to the presentation, and a Google Slides export relates a protobuf.
  const Relations relations =
      parse_relationships(*m_files, AbsPath("/ppt/presentation.xml"));
  std::vector<AbsPath> slides;
  for (const pugi::xml_node slide_id : m_document_xml.document_element()
                                           .child("p:sldIdLst")
                                           .children("p:sldId")) {
    const std::string id = slide_id.attribute("r:id").value();
    AbsPath slide_path = AbsPath("/ppt").join(RelPath(relations.at(id)));
    m_slides_xml[id] = xml::parse(*m_files, slide_path);
    slides.push_back(std::move(slide_path));
  }

  // ECMA-376 default slide size when p:sldSz is absent.
  m_slide_layout.width = Measure("10 in");
  m_slide_layout.height = Measure("7.5 in");
  if (const pugi::xml_node slide_size =
          m_document_xml.document_element().child("p:sldSz")) {
    if (const std::optional<Measure> width =
            read_emus_attribute(slide_size.attribute("cx"))) {
      m_slide_layout.width = width;
    }
    if (const std::optional<Measure> height =
            read_emus_attribute(slide_size.attribute("cy"))) {
      m_slide_layout.height = height;
    }
  }

  const ParseContext parse_context(m_slides_xml);
  m_root_element = parse_tree(m_element_registry, parse_context,
                              m_document_xml.document_element());

  load_slide_styles_(slides);

  m_element_adapter = create_element_adapter(*this, m_element_registry);
}

/// slide → layout → master → theme; the master's `p:clrMap` says which slot
/// each name stands for. Layouts are shared, so a layout, its master and its
/// theme are read once rather than once per slide.
void Document::load_slide_styles_(const std::vector<AbsPath> &slides) {
  if (m_root_element == null_element_id) {
    return;
  }

  std::unordered_map<std::string, LayoutStyle> by_layout;

  // `parse_presentation_children` appends one slide per `p:sldId`, so the
  // root's children carry the same order `slides` was built in.
  ElementIdentifier slide_id =
      m_element_registry.element_at(m_root_element).first_child_id;
  for (const AbsPath &slide_path : slides) {
    if (slide_id == null_element_id) {
      break;
    }
    const ElementIdentifier current_id = slide_id;
    const pugi::xml_node slide_node =
        m_element_registry.element_at(current_id).node;
    slide_id = m_element_registry.element_at(current_id).next_sibling_id;

    const std::optional<AbsPath> layout_path =
        parse_relationship_target(*m_files, slide_path, "slideLayout");
    if (!layout_path.has_value()) {
      continue;
    }
    const auto [layout_it, inserted] =
        by_layout.try_emplace(layout_path->string());
    if (inserted) {
      layout_it->second =
          load_layout_style(*m_files, *layout_path, m_color_schemes);
    }
    const LayoutStyle &layout_style = layout_it->second;

    if (layout_style.color_scheme != nullptr) {
      m_slide_color_schemes[current_id] = layout_style.color_scheme;
    }

    std::optional<Color> background = layout_style.background;
    if (const std::optional<std::optional<Color>> stated =
            read_background_color(slide_node, layout_style.color_scheme)) {
      background = *stated;
    }
    if (background.has_value()) {
      m_slide_backgrounds[current_id] = *background;
    }
  }
}

const ColorScheme *
Document::slide_color_scheme(const ElementIdentifier element_id) const {
  const auto it = m_slide_color_schemes.find(element_id);
  return it == std::end(m_slide_color_schemes) ? nullptr : it->second;
}

PageLayout
Document::slide_page_layout(const ElementIdentifier element_id) const {
  PageLayout result = m_slide_layout;
  if (const auto it = m_slide_backgrounds.find(element_id);
      it != std::end(m_slide_backgrounds)) {
    result.background_color = it->second;
  }
  return result;
}

const ElementRegistry &Document::element_registry() const {
  return m_element_registry;
}

bool Document::is_editable() const noexcept { return false; }

bool Document::is_savable(const bool /*encrypted*/) const noexcept {
  return false;
}

void Document::save(std::ostream & /*out*/) const {
  throw UnsupportedOperation();
}

void Document::save(std::ostream & /*out*/, const char * /*password*/) const {
  throw UnsupportedOperation();
}

namespace {

class ElementAdapter final : public abstract::ElementAdapter,
                             public abstract::SlideAdapter,
                             public abstract::LineBreakAdapter,
                             public abstract::ParagraphAdapter,
                             public abstract::SpanAdapter,
                             public abstract::TextAdapter,
                             public abstract::LinkAdapter,
                             public abstract::BookmarkAdapter,
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
    return m_registry->element_at(element_id).type;
  }

  [[nodiscard]] ElementIdentifier
  element_parent(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).parent_id;
  }
  [[nodiscard]] ElementIdentifier
  element_first_child(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).first_child_id;
  }
  [[nodiscard]] ElementIdentifier
  element_last_child(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).last_child_id;
  }
  [[nodiscard]] ElementIdentifier
  element_previous_sibling(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).previous_sibling_id;
  }
  [[nodiscard]] ElementIdentifier
  element_next_sibling(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).next_sibling_id;
  }

  [[nodiscard]] bool element_is_unique(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return true;
  }
  [[nodiscard]] bool element_is_self_locatable(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return true;
  }
  [[nodiscard]] bool element_is_editable(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    // read-only; text_set_content below stays dormant until save is wired up
    return false;
  }
  [[nodiscard]]
  DocumentPath
  element_document_path(const ElementIdentifier element_id) const override {
    return util::document::extract_path(*this, element_id, null_element_id);
  }
  [[nodiscard]] ElementIdentifier
  element_navigate_path(const ElementIdentifier element_id,
                        const DocumentPath &path) const override {
    return util::document::navigate_path(*this, element_id, path);
  }

  [[nodiscard]] const SlideAdapter *
  slide_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::slide ? this : nullptr;
  }
  [[nodiscard]] const LineBreakAdapter *
  line_break_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::line_break ? this : nullptr;
  }
  [[nodiscard]] const ParagraphAdapter *
  paragraph_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::paragraph ? this : nullptr;
  }
  [[nodiscard]] const SpanAdapter *
  span_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::span ? this : nullptr;
  }
  [[nodiscard]] const TextAdapter *
  text_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::text ? this : nullptr;
  }
  [[nodiscard]] const LinkAdapter *
  link_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::link ? this : nullptr;
  }
  [[nodiscard]] const BookmarkAdapter *
  bookmark_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::bookmark ? this : nullptr;
  }
  [[nodiscard]] const TableAdapter *
  table_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::table ? this : nullptr;
  }
  [[nodiscard]] const TableColumnAdapter *
  table_column_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::table_column ? this
                                                                 : nullptr;
  }
  [[nodiscard]] const TableRowAdapter *
  table_row_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::table_row ? this : nullptr;
  }
  [[nodiscard]] const TableCellAdapter *
  table_cell_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::table_cell ? this : nullptr;
  }
  [[nodiscard]] const FrameAdapter *
  frame_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::frame ? this : nullptr;
  }
  [[nodiscard]] const ImageAdapter *
  image_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::image ? this : nullptr;
  }

  [[nodiscard]] PageLayout
  slide_page_layout(const ElementIdentifier element_id) const override {
    return m_document->slide_page_layout(element_id);
  }
  [[nodiscard]] ElementIdentifier slide_master_page(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {}; // TODO
  }
  [[nodiscard]] std::string
  slide_name(const ElementIdentifier element_id) const override {
    return get_node(element_id).child("p:cSld").attribute("name").value();
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
  void text_set_content(const ElementIdentifier element_id,
                        const std::string &text) const override {
    ElementRegistry::Element &element = m_registry->element_at(element_id);
    ElementRegistry::Text &text_element =
        m_registry->text_element_at(element_id);

    const pugi::xml_node first = get_node(element_id);
    const pugi::xml_node last = text_element.last;

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

    for (const xml::StringToken &token : xml::tokenize_text(text)) {
      switch (token.type) {
      case xml::StringToken::Type::none:
        break;
      case xml::StringToken::Type::string: {
        auto text_node = insert_node("a:t");
        text_node.append_child(pugi::xml_node_type::node_pcdata)
            .text()
            .set(token.string.c_str());
      } break;
      case xml::StringToken::Type::spaces: {
        auto text_node = insert_node("a:t");
        text_node.append_attribute("xml:space").set_value("preserve");
        text_node.append_child(pugi::xml_node_type::node_pcdata)
            .text()
            .set(token.string.c_str());
      } break;
      case xml::StringToken::Type::tabs: {
        for (std::size_t i = 0; i < token.string.size(); ++i) {
          insert_node("a:tab");
        }
      } break;
      }
    }

    if (new_first == old_first) {
      // empty text still needs a live node to anchor the element to, or the
      // removal below would leave the registry pointing at freed nodes
      insert_node("a:t");
    }

    element.node = new_first;
    text_element.last = new_last;

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

  [[nodiscard]] std::string link_href(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {}; // TODO
  }

  [[nodiscard]] std::string
  bookmark_name(const ElementIdentifier element_id) const override {
    return get_node(element_id).attribute("text:name").value();
  }

  [[nodiscard]] TableDimensions
  table_dimensions(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);

    TableDimensions result;
    result.columns = static_cast<std::uint32_t>(
        std::ranges::distance(node.child("a:tblGrid").children("a:gridCol")));
    result.rows = static_cast<std::uint32_t>(
        std::ranges::distance(node.children("a:tr")));
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
    TableColumnStyle result;
    if (const std::optional<Measure> width =
            read_emus_attribute(get_node(element_id).attribute("w"))) {
      result.width = width;
    }
    return result;
  }

  [[nodiscard]] TableRowStyle
  table_row_style(const ElementIdentifier element_id) const override {
    TableRowStyle result;
    if (const std::optional<Measure> height =
            read_emus_attribute(get_node(element_id).attribute("h"))) {
      result.height = height;
    }
    return result;
  }

  [[nodiscard]] bool
  table_cell_is_covered(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("hMerge").as_bool() ||
           node.attribute("vMerge").as_bool();
  }
  [[nodiscard]] TableDimensions
  table_cell_span(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return {node.attribute("rowSpan").as_uint(1),
            node.attribute("gridSpan").as_uint(1)};
  }
  [[nodiscard]] ValueType table_cell_value_type(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return ValueType::string;
  }
  [[nodiscard]] TableCellStyle
  table_cell_style(const ElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_cell_style;
  }

  [[nodiscard]] AnchorType frame_anchor_type(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return AnchorType::at_page;
  }
  [[nodiscard]] std::optional<Measure>
  frame_x(const ElementIdentifier element_id) const override {
    return read_emus_attribute(
        get_frame_xfrm(element_id).child("a:off").attribute("x"));
  }
  [[nodiscard]] std::optional<Measure>
  frame_y(const ElementIdentifier element_id) const override {
    return read_emus_attribute(
        get_frame_xfrm(element_id).child("a:off").attribute("y"));
  }
  [[nodiscard]] std::optional<Measure>
  frame_width(const ElementIdentifier element_id) const override {
    return read_emus_attribute(
        get_frame_xfrm(element_id).child("a:ext").attribute("cx"));
  }
  [[nodiscard]] std::optional<Measure>
  frame_height(const ElementIdentifier element_id) const override {
    return read_emus_attribute(
        get_frame_xfrm(element_id).child("a:ext").attribute("cy"));
  }
  [[nodiscard]] std::optional<std::int32_t> frame_z_index(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return std::nullopt;
  }
  [[nodiscard]] GraphicStyle
  frame_style(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    const pugi::xml_node shape_properties = node.child("p:spPr");

    GraphicStyle result;
    if (shape_properties.child("a:noFill")) {
      result.fill_color = Color(0, 0, 0, 0);
    } else if (const std::optional<Color> fill_color =
                   read_drawing_color(shape_properties.child("a:solidFill"),
                                      get_color_scheme(element_id))) {
      result.fill_color = fill_color;
    }
    if (const std::optional<VerticalAlign> vertical_align =
            read_drawing_vertical_align_attribute(
                node.child("p:txBody").child("a:bodyPr").attribute("anchor"))) {
      result.vertical_align = vertical_align;
    }
    return result;
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
    return get_node(element_id).attribute("xlink:href").value();
  }

private:
  const Document *m_document{nullptr};
  ElementRegistry *m_registry{nullptr};

  [[nodiscard]] pugi::xml_node
  get_node(const ElementIdentifier element_id) const {
    return m_registry->element_at(element_id).node;
  }

  /// The scheme of the slide the element sits on.
  [[nodiscard]] const ColorScheme *
  get_color_scheme(const ElementIdentifier element_id) const {
    for (ElementIdentifier id = element_id; id != null_element_id;
         id = element_parent(id)) {
      if (element_type(id) == ElementType::slide) {
        return m_document->slide_color_scheme(id);
      }
    }
    return nullptr;
  }

  /// `p:sp` carries its transform in `p:spPr/a:xfrm`, `p:graphicFrame` in
  /// `p:xfrm`.
  [[nodiscard]] pugi::xml_node
  get_frame_xfrm(const ElementIdentifier element_id) const {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_node xfrm = node.child("p:spPr").child("a:xfrm")) {
      return xfrm;
    }
    return node.child("p:xfrm");
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
    const ElementRegistry::Element &element =
        m_registry->element_at(element_id);
    if (element.type == ElementType::paragraph) {
      ResolvedStyle result;
      resolve_text_style(element.node, get_color_scheme(element_id),
                         result.text_style);
      resolve_paragraph_style(element.node, result.paragraph_style);
      return result;
    }
    if (element.type == ElementType::span) {
      ResolvedStyle result;
      resolve_text_style(element.node, get_color_scheme(element_id),
                         result.text_style);
      return result;
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
