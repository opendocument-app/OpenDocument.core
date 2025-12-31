#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>

#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/table_position.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_parser.hpp>
#include <odr/internal/util/document_util.hpp>
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
  const auto [workbook_xml, workbook_relations] = parse_xml_(workbook_path);
  const auto [styles_xml, _] = parse_xml_(AbsPath("/xl/styles.xml"));

  for (pugi::xml_node sheet_node :
       workbook_xml.document_element().child("sheets").children("sheet")) {
    const char *id = sheet_node.attribute("r:id").value();
    const AbsPath sheet_path =
        workbook_path.parent().join(RelPath(workbook_relations.at(id)));
    const auto [sheet_xml, sheet_relationships] = parse_xml_(sheet_path);

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

  [[nodiscard]] const SheetAdapter *
  sheet_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::sheet ? this : nullptr;
  }
  [[nodiscard]] const SheetCellAdapter *
  sheet_cell_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::sheet_cell ? this : nullptr;
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
  [[nodiscard]] const FrameAdapter *
  frame_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::frame ? this : nullptr;
  }
  [[nodiscard]] const ImageAdapter *
  image_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::image ? this : nullptr;
  }

  [[nodiscard]] std::string
  sheet_name(const ElementIdentifier element_id) const override {
    return get_node(element_id).attribute("name").value();
  }
  [[nodiscard]] TableDimensions
  sheet_dimensions(const ElementIdentifier element_id) const override {
    return m_registry->sheet_element_at(element_id).dimensions;
  }
  [[nodiscard]] TableDimensions
  sheet_content(const ElementIdentifier element_id,
                [[maybe_unused]] const std::optional<TableDimensions> range)
      const override {
    return sheet_dimensions(element_id); // TODO
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
  [[nodiscard]] TableStyle sheet_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {}; // TODO
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
    if (const pugi::xml_attribute style_attribute = cell_node.attribute("s");
        style_attribute) {
      const std::uint32_t style_id = style_attribute.as_uint();
      const ResolvedStyle style = m_style_registry->cell_style(style_id);
      result.override(style.table_cell_style);
    }
    return result;
  }

  [[nodiscard]] TablePosition
  sheet_cell_position(const ElementIdentifier element_id) const override {
    return m_registry->sheet_cell_element_at(element_id).position;
  }
  [[nodiscard]] bool sheet_cell_is_covered(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return false; // TODO
  }
  [[nodiscard]] TableDimensions sheet_cell_span(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {1, 1}; // TODO
  }
  [[nodiscard]] ValueType sheet_cell_value_type(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
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
  void
  text_set_content([[maybe_unused]] const ElementIdentifier element_id,
                   [[maybe_unused]] const std::string &text) const override {
    // TODO
  }
  [[nodiscard]] TextStyle
  text_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] std::string link_href(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {}; // TODO
  }

  [[nodiscard]] AnchorType frame_anchor_type(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
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
  [[nodiscard]] std::optional<std::string> frame_z_index(
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
    } catch (...) {
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
    return ""; // TODO
  }

private:
  const Document *m_document{nullptr};
  ElementRegistry *m_registry{nullptr};
  StyleRegistry *m_style_registry{nullptr};

  [[nodiscard]] pugi::xml_node
  get_node(const ElementIdentifier element_id) const {
    return m_registry->element_at(element_id).node;
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
create_element_adapter(const Document &document, ElementRegistry &registry,
                       StyleRegistry &style_registry) {
  return std::make_unique<ElementAdapter>(document, registry, style_registry);
}

} // namespace

} // namespace odr::internal::ooxml::spreadsheet
