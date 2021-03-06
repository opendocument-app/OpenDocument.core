#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/common/property.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_elements_impl.h>
#include <internal/odf/odf_meta.h>
#include <odr/document_elements.h>

#include <utility>

namespace odr::internal::odf {

namespace {
template <typename E, typename... Args>
std::optional<E>
create_specific_element(std::shared_ptr<const OpenDocument> document,
                        pugi::xml_node node, Args... args) {
  if (!node) {
    return {};
  }
  return E(std::move(document), node, std::forward<Args>(args)...);
}

std::optional<Element>
create_first_child(const std::shared_ptr<const OpenDocument> &document,
                   pugi::xml_node node) {
  for (auto &&c : node) {
    if (auto element = Element::create(document, c); element) {
      return element;
    }
  }
  return {};
}

std::optional<Element>
create_previous_sibling(const std::shared_ptr<const OpenDocument> &document,
                        pugi::xml_node node) {
  for (auto &&s = node.previous_sibling(); s; s = node.previous_sibling()) {
    if (auto element = Element::create(document, s); element) {
      return element;
    }
  }
  return {};
}

std::optional<Element>
create_next_sibling(const std::shared_ptr<const OpenDocument> &document,
                    pugi::xml_node node) {
  for (auto &&s = node.next_sibling(); s; s = s.next_sibling()) {
    if (auto element = Element::create(document, s); element) {
      return element;
    }
  }
  return {};
}
} // namespace

std::optional<Element>
Element::create(std::shared_ptr<const OpenDocument> document,
                pugi::xml_node node) {
  if (!node) {
    return {};
  }

  if (node.type() == pugi::node_pcdata) {
    return Element(std::move(document), node, ElementType::TEXT);
  }

  if (node.type() == pugi::node_element) {
    const std::string element = node.name();

    if (element == "text:p" || element == "text:h") {
      return Element(std::move(document), node, ElementType::PARAGRAPH);
    }
    if (element == "text:span") {
      return Element(std::move(document), node, ElementType::SPAN);
    }
    if (element == "text:s" || element == "text:tab") {
      return Element(std::move(document), node, ElementType::TEXT);
    }
    if (element == "text:line-break") {
      return Element(std::move(document), node, ElementType::LINE_BREAK);
    }
    if (element == "text:a") {
      return Element(std::move(document), node, ElementType::LINK);
    }
    if (element == "text:table-of-content") {
      // TODO
      return create(std::move(document),
                    node.child("text:index-body").first_child());
    }
    if (element == "text:bookmark" || element == "text:bookmark-start") {
      return Element(std::move(document), node, ElementType::BOOKMARK);
    }
    if (element == "text:list") {
      return Element(std::move(document), node, ElementType::LIST);
    }
    if (element == "table:table") {
      return Element(std::move(document), node, ElementType::TABLE);
    }
    if (element == "draw:frame") {
      return Element(std::move(document), node, ElementType::FRAME);
    }
    if (element == "draw:g") {
      // drawing group not supported
      return create(std::move(document), node.first_child());
    }
    if (element == "draw:image") {
      return Element(std::move(document), node, ElementType::IMAGE);
    }
    if (element == "draw:rect") {
      return Element(std::move(document), node, ElementType::RECT);
    }
    if (element == "draw:line") {
      return Element(std::move(document), node, ElementType::LINE);
    }
    if (element == "draw:circle") {
      return Element(std::move(document), node, ElementType::CIRCLE);
    }
    // TODO if (element == "draw:custom-shape")

    // TODO log element
  }

  return {};
}

Element::Element(std::shared_ptr<const OpenDocument> document,
                 const pugi::xml_node node, const ElementType type)
    : m_document{std::move(document)}, m_node{node}, m_type{type} {
  if (!node) {
    throw std::runtime_error("element does not exist");
  }
  if (type == ElementType::NONE) {
    throw std::runtime_error("invalid type: none");
  }
}

ElementType Element::type() const { return m_type; }

std::optional<Root> Element::root() const {
  return Root::create(m_document, m_node);
}

std::optional<Slide> Element::slide() const {
  if (m_type != ElementType::SLIDE) {
    return {};
  }
  return create_specific_element<Slide>(m_document, m_node);
}

std::optional<Sheet> Element::sheet() const {
  if (m_type != ElementType::SHEET) {
    return {};
  }
  return create_specific_element<Sheet>(m_document, m_node);
}

std::optional<Page> Element::page() const {
  if (m_type != ElementType::PAGE) {
    return {};
  }
  return create_specific_element<Page>(m_document, m_node);
}

std::optional<Text> Element::text() const {
  if (m_type != ElementType::TEXT) {
    return {};
  }
  return create_specific_element<Text>(m_document, m_node);
}

std::optional<SpecificElement> Element::line_break() const {
  if (m_type != ElementType::LINE_BREAK) {
    return {};
  }
  return create_specific_element<SpecificElement>(m_document, m_node);
}

std::optional<SpecificElement> Element::page_break() const {
  if (m_type != ElementType::PAGE_BREAK) {
    return {};
  }
  return create_specific_element<SpecificElement>(m_document, m_node);
}

std::optional<Paragraph> Element::paragraph() const {
  if (m_type != ElementType::PARAGRAPH) {
    return {};
  }
  return create_specific_element<Paragraph>(m_document, m_node);
}

std::optional<Span> Element::span() const {
  if (m_type != ElementType::SPAN) {
    return {};
  }
  return create_specific_element<Span>(m_document, m_node);
}

std::optional<Link> Element::link() const {
  if (m_type != ElementType::LINK) {
    return {};
  }
  return create_specific_element<Link>(m_document, m_node);
}

std::optional<Bookmark> Element::bookmark() const {
  if (m_type != ElementType::BOOKMARK) {
    return {};
  }
  return create_specific_element<Bookmark>(m_document, m_node);
}

std::optional<List> Element::list() const {
  if (m_type != ElementType::LIST) {
    return {};
  }
  return create_specific_element<List>(m_document, m_node);
}

std::optional<Table> Element::table() const {
  if (m_type != ElementType::TABLE) {
    return {};
  }
  return create_specific_element<Table>(m_document, m_node);
}

std::optional<Frame> Element::frame() const {
  if (m_type != ElementType::FRAME) {
    return {};
  }
  return create_specific_element<Frame>(m_document, m_node);
}

std::optional<Image> Element::image() const {
  if (m_type != ElementType::IMAGE) {
    return {};
  }
  return create_specific_element<Image>(m_document, m_node);
}

std::optional<Rect> Element::rect() const {
  if (m_type != ElementType::RECT) {
    return {};
  }
  return create_specific_element<Rect>(m_document, m_node);
}

std::optional<Line> Element::line() const {
  if (m_type != ElementType::LINE) {
    return {};
  }
  return create_specific_element<Line>(m_document, m_node);
}

std::optional<Circle> Element::circle() const {
  if (m_type != ElementType::CIRCLE) {
    return {};
  }
  return create_specific_element<Circle>(m_document, m_node);
}

std::optional<Element> Element::first_child() const {
  return create_first_child(m_document, m_node);
}

std::optional<Element> Element::previous_sibling() const {
  return create_previous_sibling(m_document, m_node);
}

std::optional<Element> Element::next_sibling() const {
  return create_next_sibling(m_document, m_node);
}

SpecificElement::SpecificElement(std::shared_ptr<const OpenDocument> document,
                                 pugi::xml_node node)
    : m_document{std::move(document)}, m_node{node} {}

std::optional<Element> SpecificElement::first_child() const {
  return create_first_child(m_document, m_node);
}

std::optional<Element> SpecificElement::previous_sibling() const {
  return create_previous_sibling(m_document, m_node);
}

std::optional<Element> SpecificElement::next_sibling() const {
  return create_next_sibling(m_document, m_node);
}

ResolvedStyle SpecificElement::resolved_style(const char *attribute) const {
  if (auto styleAttr = m_node.attribute(attribute); styleAttr) {
    auto style = m_document->styles().style(styleAttr.value());
    if (style) {
      return style->resolve();
    }
    // TODO log
  }
  return {};
}

std::optional<Root> Root::create(std::shared_ptr<const OpenDocument> document,
                                 pugi::xml_node node) {
  const std::string element = node.name();

  if (element == "office:text") {
    return Root(std::move(document), node, DocumentType::TEXT);
  }
  if (element == "office:presentation") {
    return Root(std::move(document), node, DocumentType::PRESENTATION);
  }
  if (element == "office:spreadsheet") {
    return Root(std::move(document), node, DocumentType::SPREADSHEET);
  }
  if (element == "office:drawing") {
    return Root(std::move(document), node, DocumentType::DRAWING);
  }

  return {};
}

Root::Root(std::shared_ptr<const OpenDocument> document, pugi::xml_node node,
           DocumentType type)
    : SpecificElement(std::move(document), node), m_type{type} {
  if (!node) {
    throw std::runtime_error("element does not exist");
  }
  if (type == DocumentType::UNKNOWN) {
    throw std::runtime_error("invalid type: none");
  }
}

std::optional<Element> Root::first_child() const {
  switch (m_type) {
  case DocumentType::TEXT:
    return Element::create(m_document, m_node.first_child());
  case DocumentType::PRESENTATION:
    return Element::create(m_document, m_node.child("draw:page"));
  case DocumentType::SPREADSHEET:
    return Element::create(m_document, m_node.child("table:table"));
  case DocumentType::DRAWING:
    return Element::create(m_document, m_node.child("draw:page"));
  default:
    return {};
  }
}

Slide::Slide(std::shared_ptr<const OpenDocument> document, pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

std::string Slide::name() const {
  return m_node.attribute("draw:name").value();
}

std::string Slide::notes() const {
  return ""; // TODO
}

std::shared_ptr<abstract::PageStyle> Slide::page_style() const {
  return m_document->styles().master_page_style(
      m_node.attribute("draw:master-page-name").value());
}

std::optional<Slide> Slide::previous_sibling() const {
  return create_specific_element<Slide>(m_document,
                                        m_node.previous_sibling("draw:page"));
}

std::optional<Slide> Slide::next_sibling() const {
  return create_specific_element<Slide>(m_document,
                                        m_node.next_sibling("draw:page"));
}

Sheet::Sheet(std::shared_ptr<const OpenDocument> document, pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

std::string Sheet::name() const {
  return m_node.attribute("table:name").value();
}

Table Sheet::table() const { return Table(m_document, m_node); }

std::optional<Sheet> Sheet::previous_sibling() const {
  return create_specific_element<Sheet>(m_document,
                                        m_node.previous_sibling("table:table"));
}

std::optional<Sheet> Sheet::next_sibling() const {
  return create_specific_element<Sheet>(m_document,
                                        m_node.next_sibling("table:table"));
}

Page::Page(std::shared_ptr<const OpenDocument> document, pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

std::string Page::name() const { return m_node.attribute("draw:name").value(); }

std::shared_ptr<abstract::PageStyle> Page::page_style() const {
  return m_document->styles().master_page_style(
      m_node.attribute("draw:master-page-name").value());
}

std::optional<Page> Page::previous_sibling() const {
  return create_specific_element<Page>(m_document,
                                       m_node.previous_sibling("draw:page"));
}

std::optional<Page> Page::next_sibling() const {
  return create_specific_element<Page>(m_document,
                                       m_node.next_sibling("draw:page"));
}

Text::Text(std::shared_ptr<const OpenDocument> document, pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

std::string Text::text() const {
  if (m_node.type() == pugi::node_pcdata) {
    return m_node.text().as_string();
  }

  const std::string element = m_node.name();
  if (element == "text:s") {
    const auto count = m_node.attribute("text:c").as_uint(1);
    return std::string(count, ' ');
  } else if (element == "text:tab") {
    return "\t";
  }

  // TODO this should never happen. log or throw?
  return "";
}

Paragraph::Paragraph(std::shared_ptr<const OpenDocument> document,
                     pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

std::shared_ptr<abstract::ParagraphStyle> Paragraph::paragraph_style() const {
  return resolved_style("text:style-name").to_paragraph_style();
}

std::shared_ptr<abstract::TextStyle> Paragraph::text_style() const {
  return resolved_style("text:style-name").to_text_style();
}

Span::Span(std::shared_ptr<const OpenDocument> document, pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

std::shared_ptr<abstract::TextStyle> Span::text_style() const {
  return resolved_style("text:style-name").to_text_style();
}

Link::Link(std::shared_ptr<const OpenDocument> document, pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

std::shared_ptr<abstract::TextStyle> Link::text_style() const {
  return resolved_style("text:style-name").to_text_style();
}

std::string Link::href() const {
  return m_node.attribute("xlink:href").value();
}

Bookmark::Bookmark(std::shared_ptr<const OpenDocument> document,
                   pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

std::string Bookmark::name() const {
  return m_node.attribute("text:name").value();
}

List::List(std::shared_ptr<const OpenDocument> document, pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

std::optional<ListItem> List::first_child() const {
  return create_specific_element<ListItem>(m_document,
                                           m_node.child("text:list-item"));
}

ListItem::ListItem(std::shared_ptr<const OpenDocument> document,
                   pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

std::optional<ListItem> ListItem::previous_sibling() const {
  return create_specific_element<ListItem>(
      m_document, m_node.previous_sibling("text:list-item"));
}

std::optional<ListItem> ListItem::next_sibling() const {
  return create_specific_element<ListItem>(
      m_document, m_node.next_sibling("text:list-item"));
}

Table::Table(std::shared_ptr<const OpenDocument> document, pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

TableDimensions Table::dimensions() const {
  std::uint32_t rows;
  std::uint32_t columns;

  estimate_table_dimensions(m_node, rows, columns);

  return {rows, columns};
}

std::optional<TableColumn> Table::first_column() const {
  return create_specific_element<TableColumn>(
      m_document, m_node.child("table:table-column"));
}

std::optional<TableRow> Table::first_row() const {
  return create_specific_element<TableRow>(
      m_document, m_node.child("table:table-row"), *this);
}

std::shared_ptr<abstract::TableStyle> Table::table_style() const {
  return resolved_style("table:style-name").to_table_style();
}

TableColumn::TableColumn(std::shared_ptr<const OpenDocument> document,
                         pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

TableColumn::TableColumn(const TableColumn &column,
                         const std::uint32_t repeat_index)
    : SpecificElement(column), m_repeat_index{repeat_index} {}

std::optional<TableColumn> TableColumn::previous_column() const {
  if (m_repeat_index > 0) {
    return TableColumn(*this, m_repeat_index - 1);
  }

  return create_specific_element<TableColumn>(
      m_document, m_node.previous_sibling("table:table-column"));
}

std::optional<TableColumn> TableColumn::next_column() const {
  const auto repeated =
      m_node.attribute("table:number-columns-repeated").as_uint(1);
  if (m_repeat_index < repeated - 1) {
    return TableColumn(*this, m_repeat_index + 1);
  }

  return create_specific_element<TableColumn>(
      m_document, m_node.next_sibling("table:table-column"));
}

std::shared_ptr<abstract::TableColumnStyle>
TableColumn::table_column_style() const {
  return resolved_style("table:style-name").to_table_column_style();
}

TableRow::TableRow(std::shared_ptr<const OpenDocument> document,
                   pugi::xml_node node, Table table)
    : SpecificElement(std::move(document), node), m_table{std::move(table)} {}

TableRow::TableRow(const TableRow &row, const std::uint32_t repeat_index)
    : SpecificElement(row), m_table{row.m_table}, m_repeat_index{repeat_index} {
}

std::optional<TableCell> TableRow::first_cell() const {
  auto first_column = m_table.first_column();
  if (!first_column) {
    return {};
  }

  return create_specific_element<TableCell>(
      m_document, m_node.child("table:table-row"), *first_column);
}

std::optional<TableRow> TableRow::previous_row() const {
  if (m_repeat_index > 0) {
    return TableRow(*this, m_repeat_index - 1);
  }

  return create_specific_element<TableRow>(
      m_document, m_node.next_sibling("table:table-row"), m_table);
}

std::optional<TableRow> TableRow::next_row() const {
  const auto repeated =
      m_node.attribute("table:number-rows-repeated").as_uint(1);
  if (m_repeat_index < repeated - 1) {
    return TableRow(*this, m_repeat_index + 1);
  }

  return create_specific_element<TableRow>(
      m_document, m_node.next_sibling("table:table-row"), m_table);
}

TableCell::TableCell(std::shared_ptr<const OpenDocument> document,
                     pugi::xml_node node, TableColumn column)
    : SpecificElement(std::move(document), node), m_column{std::move(column)} {}

TableCell::TableCell(const TableCell &cell, std::uint32_t repeat_index)
    : SpecificElement(cell), m_column{cell.m_column}, m_repeat_index{
                                                          repeat_index} {}

std::optional<TableCell> TableCell::previous_cell() const {
  if (m_repeat_index > 0) {
    return TableCell(*this, m_repeat_index - 1);
  }

  const auto previous_column = m_column.previous_column();
  if (!previous_column) {
    return {};
  }

  return create_specific_element<TableCell>(
      m_document, m_node.next_sibling("table:table-cell"), *previous_column);
}

std::optional<TableCell> TableCell::next_cell() const {
  const auto repeated =
      m_node.attribute("table:number-columns-repeated").as_uint(1);
  if (m_repeat_index < repeated - 1) {
    return TableCell(*this, m_repeat_index + 1);
  }

  const auto next_column = m_column.next_column();
  if (!next_column) {
    return {};
  }

  return create_specific_element<TableCell>(
      m_document, m_node.next_sibling("table:table-cell"), *next_column);
}

std::uint32_t TableCell::row_span() const {
  return m_node.attribute("table:number-rows-spanned").as_uint(1);
}

std::uint32_t TableCell::column_span() const {
  return m_node.attribute("table:number-columns-spanned").as_uint(1);
}

std::shared_ptr<abstract::TableCellStyle> TableCell::table_cell_style() const {
  return resolved_style("table:style-name").to_table_cell_style();
}

Frame::Frame(std::shared_ptr<const OpenDocument> document, pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

std::shared_ptr<abstract::Property> Frame::anchor_type() const {
  return std::make_shared<common::XmlAttributeProperty>(
      m_node.attribute("text:anchor-type"));
}

std::shared_ptr<abstract::Property> Frame::width() const {
  return std::make_shared<common::XmlAttributeProperty>(
      m_node.attribute("svg:width"));
}

std::shared_ptr<abstract::Property> Frame::height() const {
  return std::make_shared<common::XmlAttributeProperty>(
      m_node.attribute("svg:height"));
}

std::shared_ptr<abstract::Property> Frame::z_index() const {
  return std::make_shared<common::XmlAttributeProperty>(
      m_node.attribute("draw:z-index"));
}

Image::Image(std::shared_ptr<const OpenDocument> document, pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

bool Image::internal() const {
  const auto hrefAttr = m_node.attribute("xlink:href");
  if (!hrefAttr) {
    return false;
  }
  const std::string href = hrefAttr.value();

  try {
    const common::Path path{href};
    if (!m_document->filesystem()->is_file(path)) {
      return false;
    }

    return true;
  } catch (...) {
  }

  return false;
}

std::string Image::href() const {
  const auto hrefAttr = m_node.attribute("xlink:href");
  return hrefAttr.value();
}

std::shared_ptr<abstract::File> Image::image_file() const {
  if (!internal()) {
    // TODO support external files
    throw std::runtime_error("not internal image");
  }

  const std::string href = this->href();
  const common::Path path{href};

  return m_document->filesystem()->open(path);
}

Rect::Rect(std::shared_ptr<const OpenDocument> document, pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

std::string Rect::x() const { return m_node.attribute("svg:x").value(); }

std::string Rect::y() const { return m_node.attribute("svg:y").value(); }

std::string Rect::width() const {
  return m_node.attribute("svg:width").value();
}

std::string Rect::height() const {
  return m_node.attribute("svg:height").value();
}

std::shared_ptr<abstract::DrawingStyle> Rect::drawing_style() const {
  return resolved_style("draw:style-name").to_drawing_style();
}

Line::Line(std::shared_ptr<const OpenDocument> document, pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

std::string Line::x1() const { return m_node.attribute("svg:x1").value(); }

std::string Line::y1() const { return m_node.attribute("svg:y1").value(); }

std::string Line::x2() const { return m_node.attribute("svg:x2").value(); }

std::string Line::y2() const { return m_node.attribute("svg:y2").value(); }

std::shared_ptr<abstract::DrawingStyle> Line::drawing_style() const {
  return resolved_style("draw:style-name").to_drawing_style();
}

Circle::Circle(std::shared_ptr<const OpenDocument> document,
               pugi::xml_node node)
    : SpecificElement(std::move(document), node) {}

std::string Circle::x() const { return m_node.attribute("svg:x").value(); }

std::string Circle::y() const { return m_node.attribute("svg:y").value(); }

std::string Circle::width() const {
  return m_node.attribute("svg:width").value();
}

std::string Circle::height() const {
  return m_node.attribute("svg:height").value();
}

std::shared_ptr<abstract::DrawingStyle> Circle::drawing_style() const {
  return resolved_style("draw:style-name").to_drawing_style();
}

} // namespace odr::internal::odf
