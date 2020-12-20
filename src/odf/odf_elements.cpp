#include <access/path.h>
#include <access/storage.h>
#include <access/stream_util.h>
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

class OdfImageFile final : public common::ImageFile {
public:
  OdfImageFile(std::shared_ptr<access::ReadStorage> storage, access::Path path,
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

bool isSkipper(pugi::xml_node node) {
  // TODO this method should be removed at some point
  const std::string element = node.name();

  if (element == "office:forms")
    return true;
  if (element == "text:sequence-decls")
    return true;
  if (element == "text:bookmark-end")
    return true;

  return false;
}
} // namespace

OdfElement::OdfElement(std::shared_ptr<const OpenDocument> document,
                       std::shared_ptr<const common::Element> parent,
                       pugi::xml_node node)
    : m_document{std::move(document)}, m_parent{std::move(parent)}, m_node{
                                                                        node} {}

std::shared_ptr<const common::Element> OdfElement::parent() const {
  return m_parent;
}

std::shared_ptr<const common::Element> OdfElement::firstChild() const {
  return factorizeFirstChild(m_document, shared_from_this(), m_node);
}

std::shared_ptr<const common::Element> OdfElement::previousSibling() const {
  return factorizePreviousSibling(m_document, m_parent, m_node);
}

std::shared_ptr<const common::Element> OdfElement::nextSibling() const {
  return factorizeNextSibling(m_document, m_parent, m_node);
}

ResolvedStyle OdfElement::resolvedStyle(const char *attribute) const {
  if (auto styleAttr = m_node.attribute(attribute); styleAttr) {
    auto style = m_document->styles().style(styleAttr.value());
    if (style)
      return style->resolve();
    // TODO log
  }
  return {};
}

OdfRoot::OdfRoot(std::shared_ptr<const OpenDocument> document,
                 pugi::xml_node node)
    : OdfElement(std::move(document), nullptr, node) {}

ElementType OdfRoot::type() const { return ElementType::ROOT; }

std::shared_ptr<const common::Element> OdfRoot::parent() const { return {}; }

std::shared_ptr<const common::Element> OdfRoot::firstChild() const {
  const std::string element = m_node.name();

  if (element == "office:text")
    return factorizeFirstChild(m_document, shared_from_this(), m_node);
  if (element == "office:presentation")
    return factorizeKnownElement<OdfSlide>(m_node.child("draw:page"),
                                           m_document, shared_from_this());
  if (element == "office:spreadsheet")
    return factorizeKnownElement<OdfSheet>(m_node.child("table:table"),
                                           m_document, shared_from_this());
  if (element == "office:drawing")
    return factorizeKnownElement<OdfPage>(m_node.child("draw:page"), m_document,
                                          shared_from_this());

  return {};
}

std::shared_ptr<const common::Element> OdfRoot::previousSibling() const {
  return {};
}

std::shared_ptr<const common::Element> OdfRoot::nextSibling() const {
  return {};
}

OdfPrimitive::OdfPrimitive(std::shared_ptr<const OpenDocument> document,
                           std::shared_ptr<const common::Element> parent,
                           pugi::xml_node node, const ElementType type)
    : OdfElement(std::move(document), std::move(parent), node), m_type{type} {}

ElementType OdfPrimitive::type() const { return m_type; }

OdfSlide::OdfSlide(std::shared_ptr<const OpenDocument> document,
                   std::shared_ptr<const common::Element> parent,
                   pugi::xml_node node)
    : OdfElement(std::move(document), std::move(parent), node) {}

std::string OdfSlide::name() const {
  return m_node.attribute("draw:name").value();
}

std::string OdfSlide::notes() const {
  return ""; // TODO
}

PageProperties OdfSlide::pageProperties() const {
  return m_document->styles().masterPageProperties(
      m_node.attribute("draw:master-page-name").value());
}

OdfSheet::OdfSheet(std::shared_ptr<const OpenDocument> document,
                   std::shared_ptr<const common::Element> parent,
                   pugi::xml_node node)
    : OdfElement(std::move(document), std::move(parent), node) {}

std::string OdfSheet::name() const {
  return m_node.attribute("table:name").value();
}

std::uint32_t OdfSheet::rowCount() const {
  return 0; // TODO
}

std::uint32_t OdfSheet::columnCount() const {
  return 0; // TODO
}

std::shared_ptr<const common::Table> OdfSheet::table() const {
  return std::make_shared<OdfTable>(m_document, shared_from_this(), m_node);
}

OdfPage::OdfPage(std::shared_ptr<const OpenDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node)
    : OdfElement(std::move(document), std::move(parent), node) {}

std::string OdfPage::name() const {
  return m_node.attribute("draw:name").value();
}

PageProperties OdfPage::pageProperties() const {
  return m_document->styles().masterPageProperties(
      m_node.attribute("draw:master-page-name").value());
}

OdfTextElement::OdfTextElement(std::shared_ptr<const OpenDocument> document,
                               std::shared_ptr<const common::Element> parent,
                               pugi::xml_node node)
    : OdfElement(std::move(document), std::move(parent), node) {}

std::string OdfTextElement::text() const {
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

OdfParagraph::OdfParagraph(std::shared_ptr<const OpenDocument> document,
                           std::shared_ptr<const common::Element> parent,
                           pugi::xml_node node)
    : OdfElement(std::move(document), std::move(parent), node) {}

std::shared_ptr<common::Property> OdfParagraph::textAlign() const {
  return ResolvedStyle::lookup(
      resolvedStyle("text:style-name").paragraphProperties, "fo:text-align");
}

RectangularProperties OdfParagraph::margin() const {
  return ResolvedStyle::lookupRect(
      resolvedStyle("text:style-name").paragraphProperties, "fo:margin");
}

TextProperties OdfParagraph::textProperties() const {
  return resolvedStyle("text:style-name").toTextProperties();
}

OdfSpan::OdfSpan(std::shared_ptr<const OpenDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node)
    : OdfElement(std::move(document), std::move(parent), node) {}

TextProperties OdfSpan::textProperties() const {
  return resolvedStyle("text:style-name").toTextProperties();
}

OdfLink::OdfLink(std::shared_ptr<const OpenDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node)
    : OdfElement(std::move(document), std::move(parent), node) {}

TextProperties OdfLink::textProperties() const {
  return resolvedStyle("text:style-name").toTextProperties();
}

std::string OdfLink::href() const {
  return m_node.attribute("xlink:href").value();
}

OdfBookmark::OdfBookmark(std::shared_ptr<const OpenDocument> document,
                         std::shared_ptr<const common::Element> parent,
                         pugi::xml_node node)
    : OdfElement(std::move(document), std::move(parent), node) {}

std::string OdfBookmark::name() const {
  return m_node.attribute("text:name").value();
}

OdfList::OdfList(std::shared_ptr<const OpenDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node)
    : OdfElement(std::move(document), std::move(parent), node) {}

std::shared_ptr<const common::Element> OdfList::firstChild() const {
  return factorizeKnownElement<OdfListItem>(m_node.child("text:list-item"),
                                            m_document, shared_from_this());
}

OdfListItem::OdfListItem(std::shared_ptr<const OpenDocument> document,
                         std::shared_ptr<const common::Element> parent,
                         pugi::xml_node node)
    : OdfElement(std::move(document), std::move(parent), node) {}

std::shared_ptr<const common::Element> OdfListItem::previousSibling() const {
  return factorizeKnownElement<OdfListItem>(
      m_node.previous_sibling("text:list-item"), m_document,
      shared_from_this());
}

std::shared_ptr<const common::Element> OdfListItem::nextSibling() const {
  return factorizeKnownElement<OdfListItem>(
      m_node.next_sibling("text:list-item"), m_document, shared_from_this());
}

OdfTable::OdfTable(std::shared_ptr<const OpenDocument> document,
                   std::shared_ptr<const common::Element> parent,
                   pugi::xml_node node)
    : OdfElement(std::move(document), std::move(parent), node) {}

std::uint32_t OdfTable::rowCount() const {
  return 0; // TODO
}

std::uint32_t OdfTable::columnCount() const {
  return 0; // TODO
}

std::shared_ptr<const common::Element> OdfTable::firstChild() const {
  return {};
}

std::shared_ptr<const common::TableColumn> OdfTable::firstColumn() const {
  return factorizeKnownElement<OdfTableColumn>(
      m_node.child("table:table-column"), m_document,
      std::static_pointer_cast<const OdfTable>(shared_from_this()));
}

std::shared_ptr<const common::TableRow> OdfTable::firstRow() const {
  return factorizeKnownElement<OdfTableRow>(
      m_node.child("table:table-row"), m_document,
      std::static_pointer_cast<const OdfTable>(shared_from_this()));
}

std::shared_ptr<common::Property> OdfTable::width() const {
  return ResolvedStyle::lookup(
      resolvedStyle("table:style-name").tableProperties, "style:width");
}

OdfTableColumn::OdfTableColumn(std::shared_ptr<const OpenDocument> document,
                               std::shared_ptr<const OdfTable> table,
                               pugi::xml_node node)
    : OdfElement(std::move(document), std::move(table), node) {}

OdfTableColumn::OdfTableColumn(const OdfTableColumn &column,
                               const std::uint32_t repeatIndex)
    : OdfElement(column), m_table{column.m_table}, m_repeatIndex{repeatIndex} {}

std::shared_ptr<const OdfTableColumn> OdfTableColumn::previousColumn() const {
  if (m_repeatIndex > 0) {
    return std::make_shared<OdfTableColumn>(*this, m_repeatIndex - 1);
  }

  return factorizeKnownElement<OdfTableColumn>(
      m_node.previous_sibling("table:table-column"), m_document, m_table);
}

std::shared_ptr<const OdfTableColumn> OdfTableColumn::nextColumn() const {
  const auto repeated =
      m_node.attribute("table:number-columns-repeated").as_uint(1);
  if (m_repeatIndex < repeated - 1) {
    return std::make_shared<OdfTableColumn>(*this, m_repeatIndex + 1);
  }

  return factorizeKnownElement<OdfTableColumn>(
      m_node.next_sibling("table:table-column"), m_document, m_table);
}

std::shared_ptr<const common::Element> OdfTableColumn::firstChild() const {
  return {};
}

std::shared_ptr<const common::Element> OdfTableColumn::previousSibling() const {
  return previousColumn();
}

std::shared_ptr<const common::Element> OdfTableColumn::nextSibling() const {
  return nextColumn();
}

std::shared_ptr<common::Property> OdfTableColumn::width() const {
  return ResolvedStyle::lookup(
      resolvedStyle("table:style-name").tableProperties, "style:column-width");
}

OdfTableRow::OdfTableRow(const OdfTableRow &row,
                         const std::uint32_t repeatIndex)
    : OdfElement(row), m_table{row.m_table}, m_repeatIndex{repeatIndex} {}

OdfTableRow::OdfTableRow(std::shared_ptr<const OpenDocument> document,
                         std::shared_ptr<const OdfTable> table,
                         pugi::xml_node node)
    : OdfElement(std::move(document), table, node), m_table{std::move(table)} {}

std::shared_ptr<const OdfTableCell> OdfTableRow::firstCell() const {
  return factorizeKnownElement<OdfTableCell>(
      m_node.child("table:table-cell"), m_document,
      std::static_pointer_cast<const OdfTableRow>(shared_from_this()),
      std::static_pointer_cast<const OdfTableColumn>(m_table->firstColumn()));
}

std::shared_ptr<const OdfTableRow> OdfTableRow::previousRow() const {
  if (m_repeatIndex > 0) {
    return std::make_shared<OdfTableRow>(*this, m_repeatIndex - 1);
  }

  return factorizeKnownElement<OdfTableRow>(
      m_node.previous_sibling("table:table-row"), m_document, m_table);
}

std::shared_ptr<const OdfTableRow> OdfTableRow::nextRow() const {
  const auto repeated =
      m_node.attribute("table:number-rows-repeated").as_uint(1);
  if (m_repeatIndex < repeated - 1) {
    return std::make_shared<OdfTableRow>(*this, m_repeatIndex + 1);
  }

  return factorizeKnownElement<OdfTableRow>(
      m_node.next_sibling("table:table-row"), m_document, m_table);
}

std::shared_ptr<const common::Element> OdfTableRow::firstChild() const {
  return firstCell();
}

std::shared_ptr<const common::Element> OdfTableRow::previousSibling() const {
  return previousRow();
}

std::shared_ptr<const common::Element> OdfTableRow::nextSibling() const {
  return nextRow();
}

OdfTableCell::OdfTableCell(const OdfTableCell &cell, std::uint32_t repeatIndex)
    : OdfElement(cell), m_row{cell.m_row}, m_column{cell.m_column},
      m_repeatIndex{repeatIndex} {}

OdfTableCell::OdfTableCell(std::shared_ptr<const OpenDocument> document,
                           std::shared_ptr<const OdfTableRow> row,
                           std::shared_ptr<const OdfTableColumn> column,
                           pugi::xml_node node)
    : OdfElement(std::move(document), row, node), m_row{std::move(row)},
      m_column{std::move(column)} {}

std::shared_ptr<const OdfTableCell> OdfTableCell::previousCell() const {
  if (m_repeatIndex > 0) {
    return std::make_shared<OdfTableCell>(*this, m_repeatIndex - 1);
  }

  const auto previousColumn = m_column->previousColumn();
  if (!previousColumn)
    return {};

  return factorizeKnownElement<OdfTableCell>(
      m_node.previous_sibling("table:table-cell"), m_document, m_row,
      previousColumn);
}

std::shared_ptr<const OdfTableCell> OdfTableCell::nextCell() const {
  const auto repeated =
      m_node.attribute("table:number-columns-repeated").as_uint(1);
  if (m_repeatIndex < repeated - 1) {
    return std::make_shared<OdfTableCell>(*this, m_repeatIndex + 1);
  }

  const auto nextColumn = m_column->nextColumn();
  if (!nextColumn)
    return {};

  return factorizeKnownElement<OdfTableCell>(
      m_node.next_sibling("table:table-cell"), m_document, m_row, nextColumn);
}

std::shared_ptr<const common::Element> OdfTableCell::previousSibling() const {
  return previousCell();
}

std::shared_ptr<const common::Element> OdfTableCell::nextSibling() const {
  return nextCell();
}

std::uint32_t OdfTableCell::rowSpan() const {
  return m_node.attribute("table:number-rows-spanned").as_uint(1);
}

std::uint32_t OdfTableCell::columnSpan() const {
  return m_node.attribute("table:number-columns-spanned").as_uint(1);
}

RectangularProperties OdfTableCell::padding() const {
  return ResolvedStyle::lookupRect(
      resolvedStyle("table:style-name").tableProperties, "fo:padding");
}

RectangularProperties OdfTableCell::border() const {
  return ResolvedStyle::lookupRect(
      resolvedStyle("table:style-name").tableProperties, "fo:border");
}

OdfFrame::OdfFrame(std::shared_ptr<const OpenDocument> document,
                   std::shared_ptr<const common::Element> parent,
                   pugi::xml_node node)
    : OdfElement(std::move(document), std::move(parent), node) {}

std::shared_ptr<common::Property> OdfFrame::anchorType() const {
  return std::make_shared<common::XmlAttributeProperty>(
      m_node.attribute("text:anchor-type"));
}

std::shared_ptr<common::Property> OdfFrame::width() const {
  return std::make_shared<common::XmlAttributeProperty>(
      m_node.attribute("svg:width"));
}

std::shared_ptr<common::Property> OdfFrame::height() const {
  return std::make_shared<common::XmlAttributeProperty>(
      m_node.attribute("svg:height"));
}

std::shared_ptr<common::Property> OdfFrame::zIndex() const {
  return std::make_shared<common::XmlAttributeProperty>(
      m_node.attribute("draw:z-index"));
}

OdfImage::OdfImage(std::shared_ptr<const OpenDocument> document,
                   std::shared_ptr<const common::Element> parent,
                   pugi::xml_node node)
    : OdfElement(std::move(document), std::move(parent), node) {}

bool OdfImage::internal() const {
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

std::string OdfImage::href() const {
  const auto hrefAttr = m_node.attribute("xlink:href");
  return hrefAttr.value();
}

ImageFile OdfImage::imageFile() const {
  if (!internal())
    throw 1; // TODO

  const std::string href = this->href();
  const access::Path path{href};
  FileType fileType{FileType::UNKNOWN};

  if ((href.find("ObjectReplacements", 0) != std::string::npos) ||
      (href.find(".svm", 0) != std::string::npos)) {
    fileType = FileType::STARVIEW_METAFILE;
  }

  return ImageFile(
      std::make_shared<OdfImageFile>(m_document->storage(), path, fileType));
}

OdfDrawingElement::OdfDrawingElement(
    std::shared_ptr<const OpenDocument> document,
    std::shared_ptr<const common::Element> parent, pugi::xml_node node)
    : OdfElement(std::move(document), std::move(parent), node) {}

std::shared_ptr<common::Property> OdfDrawingElement::strokeWidth() const {
  return ResolvedStyle::lookup(
      resolvedStyle("draw:style-name").graphicProperties, "svg:stroke-width");
}

std::shared_ptr<common::Property> OdfDrawingElement::strokeColor() const {
  return ResolvedStyle::lookup(
      resolvedStyle("draw:style-name").graphicProperties, "svg:stroke-color");
}

std::shared_ptr<common::Property> OdfDrawingElement::fillColor() const {
  return ResolvedStyle::lookup(
      resolvedStyle("draw:style-name").graphicProperties, "draw:fill-color");
}

std::shared_ptr<common::Property> OdfDrawingElement::verticalAlign() const {
  return ResolvedStyle::lookup(
      resolvedStyle("draw:style-name").graphicProperties,
      "draw:textarea-vertical-align");
}

OdfRect::OdfRect(std::shared_ptr<const OpenDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node)
    : OdfDrawingElement(std::move(document), std::move(parent), node) {}

std::string OdfRect::x() const { return m_node.attribute("svg:x").value(); }

std::string OdfRect::y() const { return m_node.attribute("svg:y").value(); }

std::string OdfRect::width() const {
  return m_node.attribute("svg:width").value();
}

std::string OdfRect::height() const {
  return m_node.attribute("svg:height").value();
}

OdfLine::OdfLine(std::shared_ptr<const OpenDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node)
    : OdfDrawingElement(std::move(document), std::move(parent), node) {}

std::string OdfLine::x1() const { return m_node.attribute("svg:x1").value(); }

std::string OdfLine::y1() const { return m_node.attribute("svg:y1").value(); }

std::string OdfLine::x2() const { return m_node.attribute("svg:x2").value(); }

std::string OdfLine::y2() const { return m_node.attribute("svg:y2").value(); }

OdfCircle::OdfCircle(std::shared_ptr<const OpenDocument> document,
                     std::shared_ptr<const common::Element> parent,
                     pugi::xml_node node)
    : OdfDrawingElement(std::move(document), std::move(parent), node) {}

std::string OdfCircle::x() const { return m_node.attribute("svg:x").value(); }

std::string OdfCircle::y() const { return m_node.attribute("svg:y").value(); }

std::string OdfCircle::width() const {
  return m_node.attribute("svg:width").value();
}

std::string OdfCircle::height() const {
  return m_node.attribute("svg:height").value();
}

std::shared_ptr<common::Element>
factorizeElement(std::shared_ptr<const OpenDocument> document,
                 std::shared_ptr<const common::Element> parent,
                 pugi::xml_node node) {
  if (node.type() == pugi::node_pcdata)
    return std::make_shared<OdfTextElement>(std::move(document),
                                            std::move(parent), node);

  if (node.type() == pugi::node_element) {
    const std::string element = node.name();

    if (element == "text:p" || element == "text:h")
      return std::make_shared<OdfParagraph>(std::move(document),
                                            std::move(parent), node);
    if (element == "text:span")
      return std::make_shared<OdfSpan>(std::move(document), std::move(parent),
                                       node);
    if (element == "text:s" || element == "text:tab")
      return std::make_shared<OdfTextElement>(std::move(document),
                                              std::move(parent), node);
    if (element == "text:line-break")
      return std::make_shared<OdfPrimitive>(std::move(document),
                                            std::move(parent), node,
                                            ElementType::LINE_BREAK);
    if (element == "text:a")
      return std::make_shared<OdfLink>(std::move(document), std::move(parent),
                                       node);
    if (element == "text:table-of-content")
      // TOC not fully supported
      return factorizeFirstChild(std::move(document), std::move(parent),
                                 node.child("text:index-body"));
    if (element == "text:bookmark" || element == "text:bookmark-start")
      return std::make_shared<OdfBookmark>(std::move(document),
                                           std::move(parent), node);
    if (element == "text:list")
      return std::make_shared<OdfList>(std::move(document), std::move(parent),
                                       node);
    if (element == "table:table")
      return std::make_shared<OdfTable>(std::move(document), std::move(parent),
                                        node);
    if (element == "draw:frame")
      return std::make_shared<OdfFrame>(std::move(document), std::move(parent),
                                        node);
    if (element == "draw:g")
      // drawing group not supported
      return factorizeFirstChild(std::move(document), std::move(parent), node);
    if (element == "draw:image")
      return std::make_shared<OdfImage>(std::move(document), std::move(parent),
                                        node);
    if (element == "draw:rect")
      return std::make_shared<OdfRect>(std::move(document), std::move(parent),
                                       node);
    if (element == "draw:line")
      return std::make_shared<OdfLine>(std::move(document), std::move(parent),
                                       node);
    if (element == "draw:circle")
      return std::make_shared<OdfCircle>(std::move(document), std::move(parent),
                                         node);
    // TODO if (element == "draw:custom-shape")

    // TODO log element
  }

  return nullptr;
}

std::shared_ptr<common::Element>
factorizeFirstChild(std::shared_ptr<const OpenDocument> document,
                    std::shared_ptr<const common::Element> parent,
                    pugi::xml_node node) {
  for (auto &&c : node) {
    if (isSkipper(c))
      continue;
    return factorizeElement(std::move(document), std::move(parent), c);
  }
  return {};
}

std::shared_ptr<common::Element>
factorizePreviousSibling(std::shared_ptr<const OpenDocument> document,
                         std::shared_ptr<const common::Element> parent,
                         pugi::xml_node node) {
  for (auto &&s = node.previous_sibling(); s; s = node.previous_sibling()) {
    if (isSkipper(s))
      continue;
    return factorizeElement(std::move(document), std::move(parent), s);
  }
  return {};
}

std::shared_ptr<common::Element>
factorizeNextSibling(std::shared_ptr<const OpenDocument> document,
                     std::shared_ptr<const common::Element> parent,
                     pugi::xml_node node) {
  for (auto &&s = node.next_sibling(); s; s = s.next_sibling()) {
    if (isSkipper(s))
      continue;
    return factorizeElement(std::move(document), std::move(parent), s);
  }
  return {};
}

} // namespace odr::odf
