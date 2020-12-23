#include <access/path.h>
#include <access/storage.h>
#include <common/file.h>
#include <common/xml_properties.h>
#include <odf/odf_document.h>
#include <odf/odf_document_file.h>
#include <odf/odf_elements.h>

namespace odr::odf {

// TODO possible refactor for table impl: use an internal table iterator to
// retrieve columns, rows and cells there was an implementation
// https://github.com/opendocument-app/OpenDocument.core/blob/1c30b9ed01fad491cc9ba6356f5ec6e49562eebe/common/include/common/TableCursor.h

namespace {
template <typename E, typename... Args>
std::shared_ptr<E> factorizeKnownElement(pugi::xml_node node, Args... args) {
  if (!node)
    return {};
  return std::make_shared<E>(std::forward<Args>(args)..., node);
}

class ImageFile final : public common::ImageFile {
public:
  ImageFile(std::shared_ptr<access::ReadStorage> storage, access::Path path,
            const FileType fileType)
      : m_storage{std::move(storage)}, m_path{std::move(path)}, m_fileType{
                                                                    fileType} {}

  FileType fileType() const noexcept final { return m_fileType; }

  FileMeta fileMeta() const noexcept final {
    FileMeta result;
    result.type = fileType();
    return result;
  }

  std::unique_ptr<std::istream> data() const final {
    return m_storage->read(m_path);
  }

private:
  std::shared_ptr<access::ReadStorage> m_storage;
  access::Path m_path;
  FileType m_fileType;
};

class Element : public virtual common::Element,
                public std::enable_shared_from_this<Element> {
public:
  Element(std::shared_ptr<const OpenDocument> document,
          std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : m_document{std::move(document)}, m_parent{std::move(parent)},
        m_node{node} {}

  std::shared_ptr<const common::Element> parent() const override {
    return m_parent;
  }

  std::shared_ptr<const common::Element> firstChild() const override {
    return factorizeFirstChild(m_document, shared_from_this(), m_node);
  }

  std::shared_ptr<const common::Element> previousSibling() const override {
    return factorizePreviousSibling(m_document, m_parent, m_node);
  }

  std::shared_ptr<const common::Element> nextSibling() const override {
    return factorizeNextSibling(m_document, m_parent, m_node);
  }

protected:
  const std::shared_ptr<const OpenDocument> m_document;
  const std::shared_ptr<const common::Element> m_parent;
  const pugi::xml_node m_node;

  ResolvedStyle resolvedStyle(const char *attribute) const {
    if (auto styleAttr = m_node.attribute(attribute); styleAttr) {
      auto style = m_document->styles().style(styleAttr.value());
      if (style)
        return style->resolve();
      // TODO log
    }
    return {};
  }
};

class Primitive final : public Element {
public:
  Primitive(std::shared_ptr<const OpenDocument> document,
            std::shared_ptr<const common::Element> parent, pugi::xml_node node,
            const ElementType type)
      : Element(std::move(document), std::move(parent), node), m_type{type} {}

  ElementType type() const final { return m_type; }

private:
  const ElementType m_type;
};

class TextElement final : public Element, public common::TextElement {
public:
  TextElement(std::shared_ptr<const OpenDocument> document,
              std::shared_ptr<const common::Element> parent,
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

class Paragraph final : public Element, public common::Paragraph {
public:
  Paragraph(std::shared_ptr<const OpenDocument> document,
            std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<common::ParagraphStyle> paragraphStyle() const final {
    return resolvedStyle("text:style-name").toParagraphStyle();
  }

  std::shared_ptr<common::TextStyle> textStyle() const final {
    return resolvedStyle("text:style-name").toTextStyle();
  }
};

class Span final : public Element, public common::Span {
public:
  Span(std::shared_ptr<const OpenDocument> document,
       std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<common::TextStyle> textStyle() const final {
    return resolvedStyle("text:style-name").toTextStyle();
  }
};

class Link final : public Element, public common::Link {
public:
  Link(std::shared_ptr<const OpenDocument> document,
       std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<common::TextStyle> textStyle() const final {
    return resolvedStyle("text:style-name").toTextStyle();
  }

  std::string href() const final {
    return m_node.attribute("xlink:href").value();
  }
};

class Bookmark final : public Element, public common::Bookmark {
public:
  Bookmark(std::shared_ptr<const OpenDocument> document,
           std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string name() const final {
    return m_node.attribute("text:name").value();
  }
};

class ListItem final : public Element, public common::ListItem {
public:
  ListItem(std::shared_ptr<const OpenDocument> document,
           std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<const common::Element> previousSibling() const final {
    return factorizeKnownElement<ListItem>(
        m_node.previous_sibling("text:list-item"), m_document,
        shared_from_this());
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
    return factorizeKnownElement<ListItem>(
        m_node.next_sibling("text:list-item"), m_document, shared_from_this());
  }
};

class List final : public Element, public common::List {
public:
  List(std::shared_ptr<const OpenDocument> document,
       std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<const common::Element> firstChild() const final {
    return factorizeKnownElement<ListItem>(m_node.child("text:list-item"),
                                           m_document, shared_from_this());
  }
};

class TableColumn final : public Element, public common::TableColumn {
public:
  TableColumn(std::shared_ptr<const OpenDocument> document,
              std::shared_ptr<const common::Table> table, pugi::xml_node node)
      : Element(std::move(document), std::move(table), node) {}

  TableColumn(const TableColumn &column, const std::uint32_t repeatIndex)
      : Element(column), m_table{column.m_table}, m_repeatIndex{repeatIndex} {}

  std::shared_ptr<const TableColumn> previousColumn() const {
    if (m_repeatIndex > 0) {
      return std::make_shared<TableColumn>(*this, m_repeatIndex - 1);
    }

    return factorizeKnownElement<TableColumn>(
        m_node.previous_sibling("table:table-column"), m_document, m_table);
  }

  std::shared_ptr<const TableColumn> nextColumn() const {
    const auto repeated =
        m_node.attribute("table:number-columns-repeated").as_uint(1);
    if (m_repeatIndex < repeated - 1) {
      return std::make_shared<TableColumn>(*this, m_repeatIndex + 1);
    }

    return factorizeKnownElement<TableColumn>(
        m_node.next_sibling("table:table-column"), m_document, m_table);
  }

  std::shared_ptr<const common::Element> firstChild() const final { return {}; }

  std::shared_ptr<const common::Element> previousSibling() const final {
    return previousColumn();
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
    return nextColumn();
  }

  std::shared_ptr<common::TableColumnStyle> tableColumnStyle() const final {
    return resolvedStyle("table:style-name").toTableColumnStyle();
  }

private:
  std::shared_ptr<const common::Table> m_table;
  std::uint32_t m_repeatIndex{0};
};

class TableCell final : public Element, public common::TableCell {
public:
  TableCell(const TableCell &cell, std::uint32_t repeatIndex)
      : Element(cell), m_row{cell.m_row}, m_column{cell.m_column},
        m_repeatIndex{repeatIndex} {}

  TableCell(std::shared_ptr<const OpenDocument> document,
            std::shared_ptr<const Element> row,
            std::shared_ptr<const TableColumn> column, pugi::xml_node node)
      : Element(std::move(document), row, node), m_row{std::move(row)},
        m_column{std::move(column)} {}

  std::shared_ptr<const TableCell> previousCell() const {
    if (m_repeatIndex > 0) {
      return std::make_shared<TableCell>(*this, m_repeatIndex - 1);
    }

    const auto previousColumn = m_column->previousColumn();
    if (!previousColumn)
      return {};

    return factorizeKnownElement<TableCell>(
        m_node.previous_sibling("table:table-cell"), m_document, m_row,
        previousColumn);
  }

  std::shared_ptr<const TableCell> nextCell() const {
    const auto repeated =
        m_node.attribute("table:number-columns-repeated").as_uint(1);
    if (m_repeatIndex < repeated - 1) {
      return std::make_shared<TableCell>(*this, m_repeatIndex + 1);
    }

    const auto nextColumn = m_column->nextColumn();
    if (!nextColumn)
      return {};

    return factorizeKnownElement<TableCell>(
        m_node.next_sibling("table:table-cell"), m_document, m_row, nextColumn);
  }

  std::shared_ptr<const common::Element> previousSibling() const final {
    return previousCell();
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
    return nextCell();
  }

  std::uint32_t rowSpan() const final {
    return m_node.attribute("table:number-rows-spanned").as_uint(1);
  }

  std::uint32_t columnSpan() const final {
    return m_node.attribute("table:number-columns-spanned").as_uint(1);
  }

  std::shared_ptr<common::TableCellStyle> tableCellStyle() const final {
    return resolvedStyle("table:style-name").toTableCellStyle();
  }

private:
  std::shared_ptr<const Element> m_row;
  std::shared_ptr<const TableColumn> m_column;
  std::uint32_t m_repeatIndex{0};
};

class TableRow final : public Element, public common::TableRow {
public:
  TableRow(const TableRow &row, const std::uint32_t repeatIndex)
      : Element(row), m_table{row.m_table}, m_repeatIndex{repeatIndex} {}

  TableRow(std::shared_ptr<const OpenDocument> document,
           std::shared_ptr<const common::Table> table, pugi::xml_node node)
      : Element(std::move(document), table, node), m_table{std::move(table)} {}

  std::shared_ptr<const TableCell> firstCell() const {
    return factorizeKnownElement<TableCell>(
        m_node.child("table:table-cell"), m_document,
        std::static_pointer_cast<const TableRow>(shared_from_this()),
        std::static_pointer_cast<const TableColumn>(m_table->firstColumn()));
  }

  std::shared_ptr<const TableRow> previousRow() const {
    if (m_repeatIndex > 0) {
      return std::make_shared<TableRow>(*this, m_repeatIndex - 1);
    }

    return factorizeKnownElement<TableRow>(
        m_node.previous_sibling("table:table-row"), m_document, m_table);
  }

  std::shared_ptr<const TableRow> nextRow() const {
    const auto repeated =
        m_node.attribute("table:number-rows-repeated").as_uint(1);
    if (m_repeatIndex < repeated - 1) {
      return std::make_shared<TableRow>(*this, m_repeatIndex + 1);
    }

    return factorizeKnownElement<TableRow>(
        m_node.next_sibling("table:table-row"), m_document, m_table);
  }

  std::shared_ptr<const common::Element> firstChild() const final {
    return firstCell();
  }

  std::shared_ptr<const common::Element> previousSibling() const final {
    return previousRow();
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
    return nextRow();
  }

private:
  std::shared_ptr<const common::Table> m_table;
  std::uint32_t m_repeatIndex{0};
};

class Table final : public Element, public common::Table {
public:
  Table(std::shared_ptr<const OpenDocument> document,
        std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::uint32_t rowCount() const final {
    return 0; // TODO
  }

  std::uint32_t columnCount() const final {
    return 0; // TODO
  }

  std::shared_ptr<const common::Element> firstChild() const final { return {}; }

  std::shared_ptr<const common::TableColumn> firstColumn() const final {
    return factorizeKnownElement<TableColumn>(
        m_node.child("table:table-column"), m_document,
        std::static_pointer_cast<const Table>(shared_from_this()));
  }

  std::shared_ptr<const common::TableRow> firstRow() const final {
    return factorizeKnownElement<TableRow>(
        m_node.child("table:table-row"), m_document,
        std::static_pointer_cast<const Table>(shared_from_this()));
  }

  std::shared_ptr<common::TableStyle> tableStyle() const final {
    return resolvedStyle("table:style-name").toTableStyle();
  }
};

class Frame final : public Element, public common::Frame {
public:
  Frame(std::shared_ptr<const OpenDocument> document,
        std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<common::Property> anchorType() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_node.attribute("text:anchor-type"));
  }

  std::shared_ptr<common::Property> width() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_node.attribute("svg:width"));
  }

  std::shared_ptr<common::Property> height() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_node.attribute("svg:height"));
  }

  std::shared_ptr<common::Property> zIndex() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_node.attribute("draw:z-index"));
  }
};

class Image final : public Element, public common::Image {
public:
  Image(std::shared_ptr<const OpenDocument> document,
        std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  bool internal() const final {
    const auto hrefAttr = m_node.attribute("xlink:href");
    if (!hrefAttr)
      return false;
    const std::string href = hrefAttr.value();

    try {
      const access::Path path{href};
      if (!m_document->storage()->isFile(path))
        return false;

      return true;
    } catch (...) {
    }

    return false;
  }

  std::string href() const final {
    const auto hrefAttr = m_node.attribute("xlink:href");
    return hrefAttr.value();
  }

  odr::ImageFile imageFile() const final {
    if (!internal())
      throw 1; // TODO

    const std::string href = this->href();
    const access::Path path{href};
    FileType fileType{FileType::UNKNOWN};

    if ((href.find("ObjectReplacements", 0) != std::string::npos) ||
        (href.find(".svm", 0) != std::string::npos)) {
      fileType = FileType::STARVIEW_METAFILE;
    }

    return odr::ImageFile(
        std::make_shared<ImageFile>(m_document->storage(), path, fileType));
  }
};

class Rect final : public Element, public common::Rect {
public:
  Rect(std::shared_ptr<const OpenDocument> document,
       std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string x() const final { return m_node.attribute("svg:x").value(); }

  std::string y() const final { return m_node.attribute("svg:y").value(); }

  std::string width() const final {
    return m_node.attribute("svg:width").value();
  }

  std::string height() const final {
    return m_node.attribute("svg:height").value();
  }

  std::shared_ptr<common::DrawingStyle> drawingStyle() const final {
    return resolvedStyle("draw:style-name").toDrawingStyle();
  }
};

class Line final : public Element, public common::Line {
public:
  Line(std::shared_ptr<const OpenDocument> document,
       std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string x1() const final { return m_node.attribute("svg:x1").value(); }

  std::string y1() const final { return m_node.attribute("svg:y1").value(); }

  std::string x2() const final { return m_node.attribute("svg:x2").value(); }

  std::string y2() const final { return m_node.attribute("svg:y2").value(); }

  std::shared_ptr<common::DrawingStyle> drawingStyle() const final {
    return resolvedStyle("draw:style-name").toDrawingStyle();
  }
};

class Circle final : public Element, public common::Circle {
public:
  Circle(std::shared_ptr<const OpenDocument> document,
         std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string x() const final { return m_node.attribute("svg:x").value(); }

  std::string y() const final { return m_node.attribute("svg:y").value(); }

  std::string width() const final {
    return m_node.attribute("svg:width").value();
  }

  std::string height() const final {
    return m_node.attribute("svg:height").value();
  }

  std::shared_ptr<common::DrawingStyle> drawingStyle() const final {
    return resolvedStyle("draw:style-name").toDrawingStyle();
  }
};

class Slide final : public Element, public common::Slide {
public:
  Slide(std::shared_ptr<const OpenDocument> document,
        std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string name() const final {
    return m_node.attribute("draw:name").value();
  }

  std::string notes() const final {
    return ""; // TODO
  }

  std::shared_ptr<common::PageStyle> pageStyle() const final {
    return m_document->styles().masterPageStyle(
        m_node.attribute("draw:master-page-name").value());
  }
};

class Sheet final : public Element, public common::Sheet {
public:
  Sheet(std::shared_ptr<const OpenDocument> document,
        std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string name() const final {
    return m_node.attribute("table:name").value();
  }

  std::uint32_t rowCount() const final {
    return 0; // TODO
  }

  std::uint32_t columnCount() const final {
    return 0; // TODO
  }

  std::shared_ptr<const common::Table> table() const final {
    return std::make_shared<Table>(m_document, shared_from_this(), m_node);
  }
};

class Page final : public Element, public common::Page {
public:
  Page(std::shared_ptr<const OpenDocument> document,
       std::shared_ptr<const common::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string name() const final {
    return m_node.attribute("draw:name").value();
  }

  std::shared_ptr<common::PageStyle> pageStyle() const final {
    return m_document->styles().masterPageStyle(
        m_node.attribute("draw:master-page-name").value());
  }
};

class Root final : public Element {
public:
  Root(std::shared_ptr<const OpenDocument> document, pugi::xml_node node)
      : Element(std::move(document), nullptr, node) {}

  ElementType type() const final { return ElementType::ROOT; }

  std::shared_ptr<const common::Element> parent() const final { return {}; }

  std::shared_ptr<const common::Element> firstChild() const final {
    const std::string element = m_node.name();

    if (element == "office:text")
      return factorizeFirstChild(m_document, shared_from_this(), m_node);
    if (element == "office:presentation")
      return factorizeKnownElement<Slide>(m_node.child("draw:page"), m_document,
                                          shared_from_this());
    if (element == "office:spreadsheet")
      return factorizeKnownElement<Sheet>(m_node.child("table:table"),
                                          m_document, shared_from_this());
    if (element == "office:drawing")
      return factorizeKnownElement<Page>(m_node.child("draw:page"), m_document,
                                         shared_from_this());

    return {};
  }

  std::shared_ptr<const common::Element> previousSibling() const final {
    return {};
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
    return {};
  }
};
} // namespace

std::shared_ptr<common::Element>
factorizeRoot(std::shared_ptr<const OpenDocument> document,
              pugi::xml_node node) {
  return std::make_shared<Root>(std::move(document), node);
}

std::shared_ptr<common::Element>
factorizeElement(std::shared_ptr<const OpenDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node) {
  if (node.type() == pugi::node_pcdata)
    return std::make_shared<TextElement>(std::move(document), std::move(parent),
                                         node);

  if (node.type() == pugi::node_element) {
    const std::string element = node.name();

    if (element == "text:p" || element == "text:h")
      return std::make_shared<Paragraph>(std::move(document), std::move(parent),
                                         node);
    if (element == "text:span")
      return std::make_shared<Span>(std::move(document), std::move(parent),
                                    node);
    if (element == "text:s" || element == "text:tab")
      return std::make_shared<TextElement>(std::move(document),
                                           std::move(parent), node);
    if (element == "text:line-break")
      return std::make_shared<Primitive>(std::move(document), std::move(parent),
                                         node, ElementType::LINE_BREAK);
    if (element == "text:a")
      return std::make_shared<Link>(std::move(document), std::move(parent),
                                    node);
    if (element == "text:table-of-content")
      // TOC not fully supported
      return factorizeFirstChild(std::move(document), std::move(parent),
                                 node.child("text:index-body"));
    if (element == "text:bookmark" || element == "text:bookmark-start")
      return std::make_shared<Bookmark>(std::move(document), std::move(parent),
                                        node);
    if (element == "text:list")
      return std::make_shared<List>(std::move(document), std::move(parent),
                                    node);
    if (element == "table:table")
      return std::make_shared<Table>(std::move(document), std::move(parent),
                                     node);
    if (element == "draw:frame")
      return std::make_shared<Frame>(std::move(document), std::move(parent),
                                     node);
    if (element == "draw:g")
      // drawing group not supported
      return factorizeFirstChild(std::move(document), std::move(parent), node);
    if (element == "draw:image")
      return std::make_shared<Image>(std::move(document), std::move(parent),
                                     node);
    if (element == "draw:rect")
      return std::make_shared<Rect>(std::move(document), std::move(parent),
                                    node);
    if (element == "draw:line")
      return std::make_shared<Line>(std::move(document), std::move(parent),
                                    node);
    if (element == "draw:circle")
      return std::make_shared<Circle>(std::move(document), std::move(parent),
                                      node);
    // TODO if (element == "draw:custom-shape")

    // TODO log element
  }

  return {};
}

std::shared_ptr<common::Element>
factorizeFirstChild(std::shared_ptr<const OpenDocument> document,
                    std::shared_ptr<const common::Element> parent,
                    pugi::xml_node node) {
  for (auto &&c : node) {
    auto element = factorizeElement(document, parent, c);
    if (element)
      return element;
  }
  return {};
}

std::shared_ptr<common::Element>
factorizePreviousSibling(std::shared_ptr<const OpenDocument> document,
                         std::shared_ptr<const common::Element> parent,
                         pugi::xml_node node) {
  for (auto &&s = node.previous_sibling(); s; s = node.previous_sibling()) {
    auto element = factorizeElement(document, parent, s);
    if (element)
      return element;
  }
  return {};
}

std::shared_ptr<common::Element>
factorizeNextSibling(std::shared_ptr<const OpenDocument> document,
                     std::shared_ptr<const common::Element> parent,
                     pugi::xml_node node) {
  for (auto &&s = node.next_sibling(); s; s = s.next_sibling()) {
    auto element = factorizeElement(document, parent, s);
    if (element)
      return element;
  }
  return {};
}

} // namespace odr::odf
