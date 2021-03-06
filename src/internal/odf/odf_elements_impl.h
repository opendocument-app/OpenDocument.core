#ifndef ODR_INTERNAL_ODF_ELEMENTS_IMPL_H
#define ODR_INTERNAL_ODF_ELEMENTS_IMPL_H

#include <memory>
#include <optional>
#include <pugixml.hpp>

namespace odr {
enum class ElementType;
enum class DocumentType;
struct TableDimensions;
} // namespace odr

namespace odr::internal::abstract {
class File;
class PageStyle;
class ParagraphStyle;
class TextStyle;
class DrawingStyle;
class TableStyle;
class TableColumnStyle;
class TableCellStyle;
class Property;
} // namespace odr::internal::abstract

namespace odr::internal::odf {
class OpenDocument;
struct ResolvedStyle;

class SpecificElement;
class Root;
class Slide;
class Sheet;
class Page;
class Text;
class Paragraph;
class Span;
class Link;
class Bookmark;
class List;
class ListItem;
class Table;
class TableColumn;
class TableRow;
class TableCell;
class Frame;
class Image;
class Rect;
class Line;
class Circle;

class Element final {
public:
  static std::optional<Element>
  create(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] std::optional<Root> root() const;
  [[nodiscard]] std::optional<Slide> slide() const;
  [[nodiscard]] std::optional<Sheet> sheet() const;
  [[nodiscard]] std::optional<Page> page() const;
  [[nodiscard]] std::optional<Text> text() const;
  [[nodiscard]] std::optional<SpecificElement> line_break() const;
  [[nodiscard]] std::optional<SpecificElement> page_break() const;
  [[nodiscard]] std::optional<Paragraph> paragraph() const;
  [[nodiscard]] std::optional<Span> span() const;
  [[nodiscard]] std::optional<Link> link() const;
  [[nodiscard]] std::optional<Bookmark> bookmark() const;
  [[nodiscard]] std::optional<List> list() const;
  [[nodiscard]] std::optional<Table> table() const;
  [[nodiscard]] std::optional<Frame> frame() const;
  [[nodiscard]] std::optional<Image> image() const;
  [[nodiscard]] std::optional<Rect> rect() const;
  [[nodiscard]] std::optional<Line> line() const;
  [[nodiscard]] std::optional<Circle> circle() const;

  [[nodiscard]] std::optional<Element> first_child() const;
  [[nodiscard]] std::optional<Element> previous_sibling() const;
  [[nodiscard]] std::optional<Element> next_sibling() const;

protected:
  Element(std::shared_ptr<const OpenDocument> document, pugi::xml_node node,
          ElementType type);

  std::shared_ptr<const OpenDocument> m_document;
  pugi::xml_node m_node;
  ElementType m_type;
};

class SpecificElement {
public:
  SpecificElement(std::shared_ptr<const OpenDocument> document,
                  pugi::xml_node node);

  [[nodiscard]] std::optional<Element> first_child() const;
  [[nodiscard]] std::optional<Element> previous_sibling() const;
  [[nodiscard]] std::optional<Element> next_sibling() const;

protected:
  std::shared_ptr<const OpenDocument> m_document;
  pugi::xml_node m_node;

  ResolvedStyle resolved_style(const char *attribute) const;
};

class Root final : private SpecificElement {
public:
  static std::optional<Root>
  create(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::optional<Element> first_child() const;

private:
  Root(std::shared_ptr<const OpenDocument> document, pugi::xml_node node,
       DocumentType type);

  DocumentType m_type;
};

class Slide final : private SpecificElement {
public:
  Slide(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::optional<Slide> previous_sibling() const;
  [[nodiscard]] std::optional<Slide> next_sibling() const;

  [[nodiscard]] std::string name() const;
  [[nodiscard]] std::string notes() const;

  [[nodiscard]] std::shared_ptr<abstract::PageStyle> page_style() const;
};

class Sheet final : private SpecificElement {
public:
  Sheet(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::string name() const;
  [[nodiscard]] Table table() const;

  [[nodiscard]] std::optional<Sheet> previous_sibling() const;
  [[nodiscard]] std::optional<Sheet> next_sibling() const;
};

class Page final : private SpecificElement {
public:
  Page(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::string name() const;
  [[nodiscard]] std::shared_ptr<abstract::PageStyle> page_style() const;

  [[nodiscard]] std::optional<Page> previous_sibling() const;
  [[nodiscard]] std::optional<Page> next_sibling() const;
};

class Text final : public SpecificElement {
public:
  Text(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::string text() const;

private:
  pugi::xml_node m_node;
};

class Paragraph final : public SpecificElement {
public:
  Paragraph(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::shared_ptr<abstract::ParagraphStyle>
  paragraph_style() const;

  [[nodiscard]] std::shared_ptr<abstract::TextStyle> text_style() const;

private:
  pugi::xml_node m_node;
};

class Span final : public SpecificElement {
public:
  Span(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::shared_ptr<abstract::TextStyle> text_style() const;
};

class Link final : public SpecificElement {
public:
  Link(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::shared_ptr<abstract::TextStyle> text_style() const;

  [[nodiscard]] std::string href() const;
};

class Bookmark final : public SpecificElement {
public:
  Bookmark(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::string name() const;
};

class List final : private SpecificElement {
public:
  List(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::optional<ListItem> first_child() const;
  using SpecificElement::next_sibling;
  using SpecificElement::previous_sibling;
};

class ListItem final : private SpecificElement {
public:
  ListItem(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  using SpecificElement::first_child;
  [[nodiscard]] std::optional<ListItem> previous_sibling() const;
  [[nodiscard]] std::optional<ListItem> next_sibling() const;
};

class Table final : private SpecificElement {
public:
  Table(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] TableDimensions dimensions() const;

  [[nodiscard]] std::optional<TableColumn> first_column() const;
  [[nodiscard]] std::optional<TableRow> first_row() const;
  using SpecificElement::next_sibling;
  using SpecificElement::previous_sibling;

  [[nodiscard]] std::shared_ptr<abstract::TableStyle> table_style() const;
};

class TableColumn final : private SpecificElement {
public:
  TableColumn(std::shared_ptr<const OpenDocument> document,
              pugi::xml_node node);
  TableColumn(const TableColumn &column, std::uint32_t repeatIndex);

  [[nodiscard]] std::optional<TableColumn> previous_column() const;
  [[nodiscard]] std::optional<TableColumn> next_column() const;

  [[nodiscard]] std::shared_ptr<abstract::TableColumnStyle>
  table_column_style() const;

private:
  std::uint32_t m_repeat_index{0};
};

class TableRow final : private SpecificElement {
public:
  TableRow(std::shared_ptr<const OpenDocument> document, pugi::xml_node node,
           Table table);
  TableRow(const TableRow &row, std::uint32_t repeatIndex);

  [[nodiscard]] std::optional<TableCell> first_cell() const;
  [[nodiscard]] std::optional<TableRow> previous_row() const;
  [[nodiscard]] std::optional<TableRow> next_row() const;

private:
  Table m_table;
  std::uint32_t m_repeat_index{0};
};

class TableCell final : private SpecificElement {
public:
  TableCell(std::shared_ptr<const OpenDocument> document, pugi::xml_node node,
            TableColumn column);
  TableCell(const TableCell &cell, std::uint32_t repeat_index);

  [[nodiscard]] std::optional<TableCell> previous_cell() const;
  [[nodiscard]] std::optional<TableCell> next_cell() const;

  [[nodiscard]] std::uint32_t row_span() const;
  [[nodiscard]] std::uint32_t column_span() const;

  [[nodiscard]] std::shared_ptr<abstract::TableCellStyle>
  table_cell_style() const;

private:
  TableColumn m_column;
  std::uint32_t m_repeat_index{0};
};

class Frame final : public SpecificElement {
public:
  Frame(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::shared_ptr<abstract::Property> anchor_type() const;
  [[nodiscard]] std::shared_ptr<abstract::Property> width() const;
  [[nodiscard]] std::shared_ptr<abstract::Property> height() const;
  [[nodiscard]] std::shared_ptr<abstract::Property> z_index() const;
};

class Image final : public SpecificElement {
public:
  Image(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] bool internal() const;
  [[nodiscard]] std::string href() const;
  [[nodiscard]] std::shared_ptr<abstract::File> image_file() const;
};

class Rect final : public SpecificElement {
public:
  Rect(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::string x() const;
  [[nodiscard]] std::string y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] std::shared_ptr<abstract::DrawingStyle> drawing_style() const;
};

class Line final : public SpecificElement {
public:
  Line(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::string x1() const;
  [[nodiscard]] std::string y1() const;
  [[nodiscard]] std::string x2() const;
  [[nodiscard]] std::string y2() const;

  [[nodiscard]] std::shared_ptr<abstract::DrawingStyle> drawing_style() const;
};

class Circle final : public SpecificElement {
public:
  Circle(std::shared_ptr<const OpenDocument> document, pugi::xml_node node);

  [[nodiscard]] std::string x() const;
  [[nodiscard]] std::string y() const;
  [[nodiscard]] std::string width() const;
  [[nodiscard]] std::string height() const;

  [[nodiscard]] std::shared_ptr<abstract::DrawingStyle> drawing_style() const;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_ELEMENTS_IMPL_H
