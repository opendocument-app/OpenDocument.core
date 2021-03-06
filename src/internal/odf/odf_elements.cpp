#include <internal/abstract/filesystem.h>
#include <internal/common/file.h>
#include <internal/common/path.h>
#include <internal/common/property.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_document_file.h>
#include <internal/odf/odf_elements.h>
#include <internal/odf/odf_meta.h>

namespace odr::internal::odf {

// TODO possible refactor for table impl: use an internal table iterator to
// retrieve columns, rows and cells there was an implementation
// https://github.com/opendocument-app/OpenDocument.core/blob/1c30b9ed01fad491cc9ba6356f5ec6e49562eebe/common/include/common/TableCursor.h

namespace {
template <typename E, typename... Args>
std::shared_ptr<E> factorize_known_element(pugi::xml_node node, Args... args) {
  if (!node) {
    return {};
  }
  return std::make_shared<E>(std::forward<Args>(args)..., node);
}

class Element : public virtual abstract::Element,
                public std::enable_shared_from_this<Element> {
public:
  Element(std::shared_ptr<const OpenDocument> document,
          std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : m_document{std::move(document)}, m_parent{std::move(parent)},
        m_node{node} {}

  std::shared_ptr<const abstract::Element> parent() const override {
    return m_parent;
  }

  std::shared_ptr<const abstract::Element> first_child() const override {
    return factorize_first_child(m_document, shared_from_this(), m_node);
  }

  std::shared_ptr<const abstract::Element> previous_sibling() const override {
    return factorize_previous_sibling(m_document, m_parent, m_node);
  }

  std::shared_ptr<const abstract::Element> next_sibling() const override {
    return factorize_next_sibling(m_document, m_parent, m_node);
  }

protected:
  const std::shared_ptr<const OpenDocument> m_document;
  const std::shared_ptr<const abstract::Element> m_parent;
  const pugi::xml_node m_node;

  ResolvedStyle resolved_style(const char *attribute) const {
    if (auto styleAttr = m_node.attribute(attribute); styleAttr) {
      auto style = m_document->styles().style(styleAttr.value());
      if (style) {
        return style->resolve();
      }
      // TODO log
    }
    return {};
  }
};

class Primitive final : public Element {
public:
  Primitive(std::shared_ptr<const OpenDocument> document,
            std::shared_ptr<const abstract::Element> parent,
            pugi::xml_node node, const ElementType type)
      : Element(std::move(document), std::move(parent), node), m_type{type} {}

  ElementType type() const final { return m_type; }

private:
  const ElementType m_type;
};

class TextElement final : public Element, public abstract::TextElement {
public:
  TextElement(std::shared_ptr<const OpenDocument> document,
              std::shared_ptr<const abstract::Element> parent,
              pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string text() const final {
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
};

class Paragraph final : public Element, public abstract::Paragraph {
public:
  Paragraph(std::shared_ptr<const OpenDocument> document,
            std::shared_ptr<const abstract::Element> parent,
            pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<abstract::ParagraphStyle> paragraph_style() const final {
    return resolved_style("text:style-name").to_paragraph_style();
  }

  std::shared_ptr<abstract::TextStyle> text_style() const final {
    return resolved_style("text:style-name").to_text_style();
  }
};

class Span final : public Element, public abstract::Span {
public:
  Span(std::shared_ptr<const OpenDocument> document,
       std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<abstract::TextStyle> text_style() const final {
    return resolved_style("text:style-name").to_text_style();
  }
};

class Link final : public Element, public abstract::Link {
public:
  Link(std::shared_ptr<const OpenDocument> document,
       std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<abstract::TextStyle> text_style() const final {
    return resolved_style("text:style-name").to_text_style();
  }

  std::string href() const final {
    return m_node.attribute("xlink:href").value();
  }
};

class Bookmark final : public Element, public abstract::Bookmark {
public:
  Bookmark(std::shared_ptr<const OpenDocument> document,
           std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string name() const final {
    return m_node.attribute("text:name").value();
  }
};

class ListItem final : public Element, public abstract::ListItem {
public:
  ListItem(std::shared_ptr<const OpenDocument> document,
           std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<const abstract::Element> previous_sibling() const final {
    return factorize_known_element<ListItem>(
        m_node.previous_sibling("text:list-item"), m_document,
        shared_from_this());
  }

  std::shared_ptr<const abstract::Element> next_sibling() const final {
    return factorize_known_element<ListItem>(
        m_node.next_sibling("text:list-item"), m_document, shared_from_this());
  }
};

class List final : public Element, public abstract::List {
public:
  List(std::shared_ptr<const OpenDocument> document,
       std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<const abstract::Element> first_child() const final {
    return factorize_known_element<ListItem>(m_node.child("text:list-item"),
                                             m_document, shared_from_this());
  }
};

class TableColumn final : public Element, public abstract::TableColumn {
public:
  TableColumn(std::shared_ptr<const OpenDocument> document,
              std::shared_ptr<const abstract::Table> table, pugi::xml_node node)
      : Element(std::move(document), std::move(table), node) {}

  TableColumn(const TableColumn &column, const std::uint32_t repeatIndex)
      : Element(column), m_table{column.m_table}, m_repeatIndex{repeatIndex} {}

  std::shared_ptr<const TableColumn> previous_column() const {
    if (m_repeatIndex > 0) {
      return std::make_shared<TableColumn>(*this, m_repeatIndex - 1);
    }

    return factorize_known_element<TableColumn>(
        m_node.previous_sibling("table:table-column"), m_document, m_table);
  }

  std::shared_ptr<const TableColumn> next_column() const {
    const auto repeated =
        m_node.attribute("table:number-columns-repeated").as_uint(1);
    if (m_repeatIndex < repeated - 1) {
      return std::make_shared<TableColumn>(*this, m_repeatIndex + 1);
    }

    return factorize_known_element<TableColumn>(
        m_node.next_sibling("table:table-column"), m_document, m_table);
  }

  std::shared_ptr<const abstract::Element> first_child() const final {
    return {};
  }

  std::shared_ptr<const abstract::Element> previous_sibling() const final {
    return previous_column();
  }

  std::shared_ptr<const abstract::Element> next_sibling() const final {
    return next_column();
  }

  std::shared_ptr<abstract::TableColumnStyle> table_column_style() const final {
    return resolved_style("table:style-name").to_table_column_style();
  }

private:
  std::shared_ptr<const abstract::Table> m_table;
  std::uint32_t m_repeatIndex{0};
};

class TableCell final : public Element, public abstract::TableCell {
public:
  TableCell(const TableCell &cell, std::uint32_t repeatIndex)
      : Element(cell), m_row{cell.m_row}, m_column{cell.m_column},
        m_repeatIndex{repeatIndex} {}

  TableCell(std::shared_ptr<const OpenDocument> document,
            std::shared_ptr<const Element> row,
            std::shared_ptr<const TableColumn> column, pugi::xml_node node)
      : Element(std::move(document), row, node), m_row{std::move(row)},
        m_column{std::move(column)} {}

  std::shared_ptr<const TableCell> previous_cell() const {
    if (m_repeatIndex > 0) {
      return std::make_shared<TableCell>(*this, m_repeatIndex - 1);
    }

    const auto previous_column = m_column->previous_column();
    if (!previous_column) {
      return {};
    }

    return factorize_known_element<TableCell>(
        m_node.previous_sibling("table:table-cell"), m_document, m_row,
        previous_column);
  }

  std::shared_ptr<const TableCell> next_cell() const {
    const auto repeated =
        m_node.attribute("table:number-columns-repeated").as_uint(1);
    if (m_repeatIndex < repeated - 1) {
      return std::make_shared<TableCell>(*this, m_repeatIndex + 1);
    }

    const auto next_column = m_column->next_column();
    if (!next_column) {
      return {};
    }

    return factorize_known_element<TableCell>(
        m_node.next_sibling("table:table-cell"), m_document, m_row,
        next_column);
  }

  std::shared_ptr<const abstract::Element> previous_sibling() const final {
    return previous_cell();
  }

  std::shared_ptr<const abstract::Element> next_sibling() const final {
    return next_cell();
  }

  std::uint32_t row_span() const final {
    return m_node.attribute("table:number-rows-spanned").as_uint(1);
  }

  std::uint32_t column_span() const final {
    return m_node.attribute("table:number-columns-spanned").as_uint(1);
  }

  std::shared_ptr<abstract::TableCellStyle> table_cell_style() const final {
    return resolved_style("table:style-name").to_table_cell_style();
  }

private:
  std::shared_ptr<const Element> m_row;
  std::shared_ptr<const TableColumn> m_column;
  std::uint32_t m_repeatIndex{0};
};

class TableRow final : public Element, public abstract::TableRow {
public:
  TableRow(const TableRow &row, const std::uint32_t repeatIndex)
      : Element(row), m_table{row.m_table}, m_repeat_index{repeatIndex} {}

  TableRow(std::shared_ptr<const OpenDocument> document,
           std::shared_ptr<const abstract::Table> table, pugi::xml_node node)
      : Element(std::move(document), table, node), m_table{std::move(table)} {}

  std::shared_ptr<const TableCell> first_cell() const {
    return factorize_known_element<TableCell>(
        m_node.child("table:table-cell"), m_document,
        std::static_pointer_cast<const TableRow>(shared_from_this()),
        std::static_pointer_cast<const TableColumn>(m_table->first_column()));
  }

  std::shared_ptr<const TableRow> previous_row() const {
    if (m_repeat_index > 0) {
      return std::make_shared<TableRow>(*this, m_repeat_index - 1);
    }

    return factorize_known_element<TableRow>(
        m_node.previous_sibling("table:table-row"), m_document, m_table);
  }

  std::shared_ptr<const TableRow> next_row() const {
    const auto repeated =
        m_node.attribute("table:number-rows-repeated").as_uint(1);
    if (m_repeat_index < repeated - 1) {
      return std::make_shared<TableRow>(*this, m_repeat_index + 1);
    }

    return factorize_known_element<TableRow>(
        m_node.next_sibling("table:table-row"), m_document, m_table);
  }

  std::shared_ptr<const abstract::Element> first_child() const final {
    return first_cell();
  }

  std::shared_ptr<const abstract::Element> previous_sibling() const final {
    return previous_row();
  }

  std::shared_ptr<const abstract::Element> next_sibling() const final {
    return next_row();
  }

private:
  std::shared_ptr<const abstract::Table> m_table;
  std::uint32_t m_repeat_index{0};
};

class Table final : public Element, public abstract::Table {
public:
  Table(std::shared_ptr<const OpenDocument> document,
        std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  TableDimensions dimensions() const final {
    std::uint32_t rows;
    std::uint32_t columns;

    estimate_table_dimensions(m_node, rows, columns);

    return {rows, columns};
  }

  std::shared_ptr<const abstract::Element> first_child() const final {
    return {};
  }

  std::shared_ptr<const abstract::TableColumn> first_column() const final {
    return factorize_known_element<TableColumn>(
        m_node.child("table:table-column"), m_document,
        std::static_pointer_cast<const Table>(shared_from_this()));
  }

  std::shared_ptr<const abstract::TableRow> first_row() const final {
    return factorize_known_element<TableRow>(
        m_node.child("table:table-row"), m_document,
        std::static_pointer_cast<const Table>(shared_from_this()));
  }

  std::shared_ptr<abstract::TableStyle> table_style() const final {
    return resolved_style("table:style-name").to_table_style();
  }
};

class Frame final : public Element, public abstract::Frame {
public:
  Frame(std::shared_ptr<const OpenDocument> document,
        std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<abstract::Property> anchor_type() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_node.attribute("text:anchor-type"));
  }

  std::shared_ptr<abstract::Property> width() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_node.attribute("svg:width"));
  }

  std::shared_ptr<abstract::Property> height() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_node.attribute("svg:height"));
  }

  std::shared_ptr<abstract::Property> z_index() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_node.attribute("draw:z-index"));
  }
};

class Image final : public Element, public abstract::Image {
public:
  Image(std::shared_ptr<const OpenDocument> document,
        std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  bool internal() const final {
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

  std::string href() const final {
    const auto hrefAttr = m_node.attribute("xlink:href");
    return hrefAttr.value();
  }

  std::shared_ptr<abstract::File> image_file() const final {
    if (!internal()) {
      // TODO support external files
      throw std::runtime_error("not internal image");
    }

    const std::string href = this->href();
    const common::Path path{href};

    return m_document->filesystem()->open(path);
  }
};

class Rect final : public Element, public abstract::Rect {
public:
  Rect(std::shared_ptr<const OpenDocument> document,
       std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string x() const final { return m_node.attribute("svg:x").value(); }

  std::string y() const final { return m_node.attribute("svg:y").value(); }

  std::string width() const final {
    return m_node.attribute("svg:width").value();
  }

  std::string height() const final {
    return m_node.attribute("svg:height").value();
  }

  std::shared_ptr<abstract::DrawingStyle> drawing_style() const final {
    return resolved_style("draw:style-name").to_drawing_style();
  }
};

class Line final : public Element, public abstract::Line {
public:
  Line(std::shared_ptr<const OpenDocument> document,
       std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string x1() const final { return m_node.attribute("svg:x1").value(); }

  std::string y1() const final { return m_node.attribute("svg:y1").value(); }

  std::string x2() const final { return m_node.attribute("svg:x2").value(); }

  std::string y2() const final { return m_node.attribute("svg:y2").value(); }

  std::shared_ptr<abstract::DrawingStyle> drawing_style() const final {
    return resolved_style("draw:style-name").to_drawing_style();
  }
};

class Circle final : public Element, public abstract::Circle {
public:
  Circle(std::shared_ptr<const OpenDocument> document,
         std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string x() const final { return m_node.attribute("svg:x").value(); }

  std::string y() const final { return m_node.attribute("svg:y").value(); }

  std::string width() const final {
    return m_node.attribute("svg:width").value();
  }

  std::string height() const final {
    return m_node.attribute("svg:height").value();
  }

  std::shared_ptr<abstract::DrawingStyle> drawing_style() const final {
    return resolved_style("draw:style-name").to_drawing_style();
  }
};

class Slide final : public Element, public abstract::Slide {
public:
  Slide(std::shared_ptr<const OpenDocument> document,
        std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string name() const final {
    return m_node.attribute("draw:name").value();
  }

  std::string notes() const final {
    return ""; // TODO
  }

  std::shared_ptr<abstract::PageStyle> page_style() const final {
    return m_document->styles().master_page_style(
        m_node.attribute("draw:master-page-name").value());
  }

  std::shared_ptr<const abstract::Element> previous_sibling() const final {
    return factorize_known_element<Slide>(m_node.previous_sibling("draw:page"),
                                          m_document, m_parent);
  }

  std::shared_ptr<const abstract::Element> next_sibling() const final {
    return factorize_known_element<Slide>(m_node.next_sibling("draw:page"),
                                          m_document, m_parent);
  }
};

class Sheet final : public Element, public abstract::Sheet {
public:
  Sheet(std::shared_ptr<const OpenDocument> document,
        std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string name() const final {
    return m_node.attribute("table:name").value();
  }

  std::shared_ptr<const abstract::Table> table() const final {
    return std::make_shared<Table>(m_document, shared_from_this(), m_node);
  }

  std::shared_ptr<const abstract::Element> previous_sibling() const final {
    return factorize_known_element<Sheet>(
        m_node.previous_sibling("table:table"), m_document, m_parent);
  }

  std::shared_ptr<const abstract::Element> next_sibling() const final {
    return factorize_known_element<Sheet>(m_node.next_sibling("table:table"),
                                          m_document, m_parent);
  }
};

class Page final : public Element, public abstract::Page {
public:
  Page(std::shared_ptr<const OpenDocument> document,
       std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string name() const final {
    return m_node.attribute("draw:name").value();
  }

  std::shared_ptr<abstract::PageStyle> page_style() const final {
    return m_document->styles().master_page_style(
        m_node.attribute("draw:master-page-name").value());
  }

  std::shared_ptr<const abstract::Element> previous_sibling() const final {
    return factorize_known_element<Page>(m_node.previous_sibling("draw:page"),
                                         m_document, m_parent);
  }

  std::shared_ptr<const abstract::Element> next_sibling() const final {
    return factorize_known_element<Page>(m_node.next_sibling("draw:page"),
                                         m_document, m_parent);
  }
};

class Root final : public Element {
public:
  Root(std::shared_ptr<const OpenDocument> document, pugi::xml_node node)
      : Element(std::move(document), nullptr, node) {}

  ElementType type() const final { return ElementType::ROOT; }

  std::shared_ptr<const abstract::Element> parent() const final { return {}; }

  std::shared_ptr<const abstract::Element> first_child() const final {
    const std::string element = m_node.name();

    if (element == "office:text") {
      return factorize_first_child(m_document, shared_from_this(), m_node);
    }
    if (element == "office:presentation") {
      return factorize_known_element<Slide>(m_node.child("draw:page"),
                                            m_document, shared_from_this());
    }
    if (element == "office:spreadsheet") {
      return factorize_known_element<Sheet>(m_node.child("table:table"),
                                            m_document, shared_from_this());
    }
    if (element == "office:drawing") {
      return factorize_known_element<Page>(m_node.child("draw:page"),
                                           m_document, shared_from_this());
    }

    return {};
  }

  std::shared_ptr<const abstract::Element> previous_sibling() const final {
    return {};
  }

  std::shared_ptr<const abstract::Element> next_sibling() const final {
    return {};
  }
};
} // namespace

std::shared_ptr<abstract::Element>
factorize_root(std::shared_ptr<const OpenDocument> document,
               pugi::xml_node node) {
  return std::make_shared<Root>(std::move(document), node);
}

std::shared_ptr<abstract::Element>
factorize_element(std::shared_ptr<const OpenDocument> document,
                  std::shared_ptr<const abstract::Element> parent,
                  pugi::xml_node node) {
  if (node.type() == pugi::node_pcdata) {
    return std::make_shared<TextElement>(std::move(document), std::move(parent),
                                         node);
  }

  if (node.type() == pugi::node_element) {
    const std::string element = node.name();

    if (element == "text:p" || element == "text:h") {
      return std::make_shared<Paragraph>(std::move(document), std::move(parent),
                                         node);
    }
    if (element == "text:span") {
      return std::make_shared<Span>(std::move(document), std::move(parent),
                                    node);
    }
    if (element == "text:s" || element == "text:tab") {
      return std::make_shared<TextElement>(std::move(document),
                                           std::move(parent), node);
    }
    if (element == "text:line-break") {
      return std::make_shared<Primitive>(std::move(document), std::move(parent),
                                         node, ElementType::LINE_BREAK);
    }
    if (element == "text:a") {
      return std::make_shared<Link>(std::move(document), std::move(parent),
                                    node);
    }
    if (element == "text:table-of-content") {
      // TOC not fully supported
      return factorize_first_child(std::move(document), std::move(parent),
                                   node.child("text:index-body"));
    }
    if (element == "text:bookmark" || element == "text:bookmark-start") {
      return std::make_shared<Bookmark>(std::move(document), std::move(parent),
                                        node);
    }
    if (element == "text:list") {
      return std::make_shared<List>(std::move(document), std::move(parent),
                                    node);
    }
    if (element == "table:table") {
      return std::make_shared<Table>(std::move(document), std::move(parent),
                                     node);
    }
    if (element == "draw:frame") {
      return std::make_shared<Frame>(std::move(document), std::move(parent),
                                     node);
    }
    if (element == "draw:g") {
      // drawing group not supported
      return factorize_first_child(std::move(document), std::move(parent),
                                   node);
    }
    if (element == "draw:image") {
      return std::make_shared<Image>(std::move(document), std::move(parent),
                                     node);
    }
    if (element == "draw:rect") {
      return std::make_shared<Rect>(std::move(document), std::move(parent),
                                    node);
    }
    if (element == "draw:line") {
      return std::make_shared<Line>(std::move(document), std::move(parent),
                                    node);
    }
    if (element == "draw:circle") {
      return std::make_shared<Circle>(std::move(document), std::move(parent),
                                      node);
    }
    // TODO if (element == "draw:custom-shape")

    // TODO log element
  }

  return {};
}

std::shared_ptr<abstract::Element>
factorize_first_child(std::shared_ptr<const OpenDocument> document,
                      std::shared_ptr<const abstract::Element> parent,
                      pugi::xml_node node) {
  for (auto &&c : node) {
    auto element = factorize_element(document, parent, c);
    if (element) {
      return element;
    }
  }
  return {};
}

std::shared_ptr<abstract::Element>
factorize_previous_sibling(std::shared_ptr<const OpenDocument> document,
                           std::shared_ptr<const abstract::Element> parent,
                           pugi::xml_node node) {
  for (auto &&s = node.previous_sibling(); s; s = node.previous_sibling()) {
    auto element = factorize_element(document, parent, s);
    if (element) {
      return element;
    }
  }
  return {};
}

std::shared_ptr<abstract::Element>
factorize_next_sibling(std::shared_ptr<const OpenDocument> document,
                       std::shared_ptr<const abstract::Element> parent,
                       pugi::xml_node node) {
  for (auto &&s = node.next_sibling(); s; s = s.next_sibling()) {
    auto element = factorize_element(document, parent, s);
    if (element) {
      return element;
    }
  }
  return {};
}

} // namespace odr::internal::odf
