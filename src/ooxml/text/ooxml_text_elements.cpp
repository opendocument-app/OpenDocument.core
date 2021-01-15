#include <abstract/storage.h>
#include <common/file.h>
#include <common/path.h>
#include <common/property.h>
#include <odr/document_elements.h>
#include <odr/file.h>
#include <ooxml/ooxml_document.h>
#include <ooxml/text/ooxml_text_elements.h>

namespace odr::ooxml::text {

namespace {
template <typename E, typename... Args>
std::shared_ptr<E> factorizeKnownElement(pugi::xml_node node, Args... args) {
  if (!node)
    return {};
  return std::make_shared<E>(std::forward<Args>(args)..., node);
}

// TODO duplication in ODF
class ImageFile final : public abstract::ImageFile {
public:
  ImageFile(std::shared_ptr<abstract::ReadStorage> storage, common::Path path,
            const FileType fileType)
      : m_storage{std::move(storage)}, m_path{std::move(path)}, m_fileType{
                                                                    fileType} {}

  FileType file_type() const noexcept final { return m_fileType; }

  FileMeta file_meta() const noexcept final {
    FileMeta result;
    result.type = file_type();
    return result;
  }

  FileLocation file_location() const noexcept final {
    return FileLocation::UNKNOWN; // TODO
  }

  std::size_t size() const final {
    return 0; // TODO
  }

  std::unique_ptr<std::istream> data() const final {
    return m_storage->read(m_path);
  }

  std::shared_ptr<abstract::Image> image() const final {
    return {}; // TODO
  }

private:
  std::shared_ptr<abstract::ReadStorage> m_storage;
  common::Path m_path;
  FileType m_fileType;
};

class Element : public virtual abstract::Element,
                public std::enable_shared_from_this<Element> {
public:
  Element(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
          std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : m_document{std::move(document)}, m_parent{std::move(parent)},
        m_node{node} {}

  std::shared_ptr<const abstract::Element> parent() const override {
    return m_parent;
  }

  std::shared_ptr<const abstract::Element> firstChild() const override {
    return factorizeFirstChild(m_document, shared_from_this(), m_node);
  }

  std::shared_ptr<const abstract::Element> previousSibling() const override {
    return factorizePreviousSibling(m_document, m_parent, m_node);
  }

  std::shared_ptr<const abstract::Element> nextSibling() const override {
    return factorizeNextSibling(m_document, m_parent, m_node);
  }

protected:
  const std::shared_ptr<const OfficeOpenXmlTextDocument> m_document;
  const std::shared_ptr<const abstract::Element> m_parent;
  const pugi::xml_node m_node;

  ResolvedStyle resolvedStyle() const { return resolvedStyle({}); }

  ResolvedStyle resolvedStyle(pugi::xml_attribute attribute) const {
    std::string styleId = attribute.value();
    auto style = m_document->styles().style(styleId);
    if (!style)
      style = std::make_shared<Style>();
    return style->resolve(m_node);
  }
};

class Root final : public Element {
public:
  Root(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
       pugi::xml_node node)
      : Element(std::move(document), nullptr, node) {}

  ElementType type() const final { return ElementType::ROOT; }

  std::shared_ptr<const abstract::Element> parent() const final { return {}; }

  std::shared_ptr<const abstract::Element> firstChild() const final {
    return factorizeFirstChild(m_document, shared_from_this(), m_node);
  }

  std::shared_ptr<const abstract::Element> previousSibling() const final {
    return {};
  }

  std::shared_ptr<const abstract::Element> nextSibling() const final {
    return {};
  }
};

class Primitive final : public Element {
public:
  Primitive(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
            std::shared_ptr<const abstract::Element> parent,
            pugi::xml_node node, const ElementType type)
      : Element(std::move(document), std::move(parent), node), m_type{type} {}

  ElementType type() const final { return m_type; }

private:
  const ElementType m_type;
};

class TextElement final : public Element, public abstract::TextElement {
public:
  TextElement(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
              std::shared_ptr<const abstract::Element> parent,
              pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string text() const final { return m_node.first_child().value(); }
};

class Paragraph final : public Element, public abstract::Paragraph {
public:
  Paragraph(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
            std::shared_ptr<const abstract::Element> parent,
            pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<abstract::ParagraphStyle> paragraphStyle() const final {
    auto styleAttr = m_node.child("w:pPr").child("w:pStyle").attribute("w:val");
    return resolvedStyle(styleAttr).toParagraphStyle();
  }

  std::shared_ptr<abstract::TextStyle> textStyle() const final {
    auto styleAttr = m_node.child("w:pPr").child("w:pStyle").attribute("w:val");
    return resolvedStyle(styleAttr).toTextStyle();
  }
};

class Span final : public Element, public abstract::Span {
public:
  Span(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
       std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<abstract::TextStyle> textStyle() const final {
    return resolvedStyle().toTextStyle();
  }
};

class Link final : public Element, public abstract::Link {
public:
  Link(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
       std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<abstract::TextStyle> textStyle() const final { return {}; }

  std::string href() const final {
    return m_node.attribute("xlink:href").value();
  }
};

class Bookmark final : public Element, public abstract::Bookmark {
public:
  Bookmark(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
           std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::string name() const final { return m_node.attribute("w:name").value(); }
};

class ListItem final : public Element, public abstract::ListItem {
public:
  ListItem(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
           std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<const abstract::Element> previousSibling() const final {
    return factorizeKnownElement<ListItem>(
        m_node.previous_sibling("text:list-item"), m_document,
        shared_from_this());
  }

  std::shared_ptr<const abstract::Element> nextSibling() const final {
    return factorizeKnownElement<ListItem>(
        m_node.next_sibling("text:list-item"), m_document, shared_from_this());
  }
};

class List final : public Element, public abstract::List {
public:
  List(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
       std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<const abstract::Element> firstChild() const final {
    return factorizeKnownElement<ListItem>(m_node.child("text:list-item"),
                                           m_document, shared_from_this());
  }
};

class TableColumn final : public Element, public abstract::TableColumn {
public:
  TableColumn(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
              std::shared_ptr<const abstract::Table> table, pugi::xml_node node)
      : Element(std::move(document), std::move(table), node) {}

  std::shared_ptr<const TableColumn> previousColumn() const {
    return factorizeKnownElement<TableColumn>(
        m_node.previous_sibling("table:table-column"), m_document, m_table);
  }

  std::shared_ptr<const TableColumn> nextColumn() const {
    return factorizeKnownElement<TableColumn>(
        m_node.next_sibling("table:table-column"), m_document, m_table);
  }

  std::shared_ptr<const abstract::Element> firstChild() const final {
    return {};
  }

  std::shared_ptr<const abstract::Element> previousSibling() const final {
    return previousColumn();
  }

  std::shared_ptr<const abstract::Element> nextSibling() const final {
    return nextColumn();
  }

  std::shared_ptr<abstract::TableColumnStyle> tableColumnStyle() const final {
    return {}; // TODO
  }

private:
  std::shared_ptr<const abstract::Table> m_table;
};

class TableCell final : public Element, public abstract::TableCell {
public:
  TableCell(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
            std::shared_ptr<const Element> row, pugi::xml_node node)
      : Element(std::move(document), row, node), m_row{std::move(row)} {}

  std::shared_ptr<const TableCell> previousCell() const {
    return factorizeKnownElement<TableCell>(m_node.previous_sibling("w:tc"),
                                            m_document, m_row);
  }

  std::shared_ptr<const TableCell> nextCell() const {
    return factorizeKnownElement<TableCell>(m_node.next_sibling("w:tc"),
                                            m_document, m_row);
  }

  std::shared_ptr<const abstract::Element> previousSibling() const final {
    return previousCell();
  }

  std::shared_ptr<const abstract::Element> nextSibling() const final {
    return nextCell();
  }

  std::uint32_t rowSpan() const final {
    // TODO
    return m_node.attribute("table:number-rows-spanned").as_uint(1);
  }

  std::uint32_t columnSpan() const final {
    // TODO
    return m_node.attribute("table:number-columns-spanned").as_uint(1);
  }

  std::shared_ptr<abstract::TableCellStyle> tableCellStyle() const final {
    return {}; // TODO
  }

private:
  std::shared_ptr<const Element> m_row;
};

class TableRow final : public Element, public abstract::TableRow {
public:
  TableRow(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
           std::shared_ptr<const abstract::Table> table, pugi::xml_node node)
      : Element(std::move(document), table, node), m_table{std::move(table)} {}

  std::shared_ptr<const TableCell> firstCell() const {
    return factorizeKnownElement<TableCell>(
        m_node.child("w:tc"), m_document,
        std::static_pointer_cast<const TableRow>(shared_from_this()));
  }

  std::shared_ptr<const TableRow> previousRow() const {
    return factorizeKnownElement<TableRow>(m_node.previous_sibling("w:tr"),
                                           m_document, m_table);
  }

  std::shared_ptr<const TableRow> nextRow() const {
    return factorizeKnownElement<TableRow>(m_node.next_sibling("w:tr"),
                                           m_document, m_table);
  }

  std::shared_ptr<const abstract::Element> firstChild() const final {
    return firstCell();
  }

  std::shared_ptr<const abstract::Element> previousSibling() const final {
    return previousRow();
  }

  std::shared_ptr<const abstract::Element> nextSibling() const final {
    return nextRow();
  }

private:
  std::shared_ptr<const abstract::Table> m_table;
};

class Table final : public Element, public abstract::Table {
public:
  Table(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
        std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::uint32_t rowCount() const final {
    return 0; // TODO
  }

  std::uint32_t columnCount() const final {
    return 0; // TODO
  }

  std::shared_ptr<const abstract::Element> firstChild() const final {
    return {};
  }

  std::shared_ptr<const abstract::TableColumn> firstColumn() const final {
    // TODO
    return factorizeKnownElement<TableColumn>(
        m_node.child("table:table-column"), m_document,
        std::static_pointer_cast<const Table>(shared_from_this()));
  }

  std::shared_ptr<const abstract::TableRow> firstRow() const final {
    return factorizeKnownElement<TableRow>(
        m_node.child("w:tr"), m_document,
        std::static_pointer_cast<const Table>(shared_from_this()));
  }

  std::shared_ptr<abstract::TableStyle> tableStyle() const final {
    return {}; // TODO
  }
};

class Frame final : public Element, public abstract::Frame {
public:
  Frame(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
        std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  std::shared_ptr<abstract::Property> anchorType() const final {
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

  std::shared_ptr<abstract::Property> zIndex() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_node.attribute("draw:z-index"));
  }
};

class Image final : public Element, public abstract::Image {
public:
  Image(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
        std::shared_ptr<const abstract::Element> parent, pugi::xml_node node)
      : Element(std::move(document), std::move(parent), node) {}

  bool internal() const final {
    const auto hrefAttr = m_node.attribute("xlink:href");
    if (!hrefAttr)
      return false;
    const std::string href = hrefAttr.value();

    try {
      const common::Path path{href};
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
    const common::Path path{href};
    FileType fileType{FileType::UNKNOWN};

    if ((href.find("ObjectReplacements", 0) != std::string::npos) ||
        (href.find(".svm", 0) != std::string::npos)) {
      fileType = FileType::STARVIEW_METAFILE;
    }

    return odr::ImageFile(
        std::make_shared<ImageFile>(m_document->storage(), path, fileType));
  }
};
} // namespace

std::shared_ptr<abstract::Element>
factorizeElement(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
                 std::shared_ptr<const abstract::Element> parent,
                 pugi::xml_node node) {
  if (node.type() == pugi::node_element) {
    const std::string element = node.name();

    if (element == "w:t")
      return std::make_shared<TextElement>(std::move(document),
                                           std::move(parent), node);
    if (element == "w:p")
      return std::make_shared<Paragraph>(std::move(document), std::move(parent),
                                         node);
    if (element == "w:r")
      return std::make_shared<Span>(std::move(document), std::move(parent),
                                    node);
    if (element == "w:hyperlink")
      return std::make_shared<Link>(std::move(document), std::move(parent),
                                    node);
    if (element == "w:bookmarkStart")
      return std::make_shared<Bookmark>(std::move(document), std::move(parent),
                                        node);
    if (element == "w:tbl")
      return std::make_shared<Table>(std::move(document), std::move(parent),
                                     node);

    // TODO log element
  }

  return {};
}

std::shared_ptr<abstract::Element>
factorizeRoot(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
              pugi::xml_node node) {
  return std::make_shared<Root>(std::move(document), node);
}

std::shared_ptr<abstract::Element>
factorizeFirstChild(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
                    std::shared_ptr<const abstract::Element> parent,
                    pugi::xml_node node) {
  for (auto &&c : node) {
    auto element = factorizeElement(document, parent, c);
    if (element)
      return element;
  }
  return {};
}

std::shared_ptr<abstract::Element> factorizePreviousSibling(
    std::shared_ptr<const OfficeOpenXmlTextDocument> document,
    std::shared_ptr<const abstract::Element> parent, pugi::xml_node node) {
  for (auto &&s = node.previous_sibling(); s; s = node.previous_sibling()) {
    auto element = factorizeElement(document, parent, s);
    if (element)
      return element;
  }
  return {};
}

std::shared_ptr<abstract::Element>
factorizeNextSibling(std::shared_ptr<const OfficeOpenXmlTextDocument> document,
                     std::shared_ptr<const abstract::Element> parent,
                     pugi::xml_node node) {
  for (auto &&s = node.next_sibling(); s; s = s.next_sibling()) {
    auto element = factorizeElement(document, parent, s);
    if (element)
      return element;
  }
  return {};
}

} // namespace odr::ooxml::text
