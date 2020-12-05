#include <access/StreamUtil.h>
#include <access/ZipStorage.h>
#include <common/DocumentElements.h>
#include <common/XmlUtil.h>
#include <odf/Crypto.h>
#include <odf/OpenDocument.h>
#include <odr/Document.h>
#include <odr/DocumentElements.h>

namespace odr::odf {

namespace {
class OdfElement;

std::shared_ptr<common::Element>
firstChildImpl(std::shared_ptr<const OpenDocument> root,
               std::shared_ptr<const OdfElement> parent, pugi::xml_node node);
std::shared_ptr<common::Element>
previousSiblingImpl(std::shared_ptr<const OpenDocument> root,
                    std::shared_ptr<const OdfElement> parent,
                    pugi::xml_node node);
std::shared_ptr<common::Element>
nextSiblingImpl(std::shared_ptr<const OpenDocument> root,
                std::shared_ptr<const OdfElement> parent, pugi::xml_node node);

template<typename E, typename ...Args>
std::shared_ptr<E>
knownElementImpl(std::shared_ptr<const OpenDocument> root,
                    std::shared_ptr<const OdfElement> parent, pugi::xml_node node) {
  if (!node) return {};
  return std::make_shared<E>(std::move(root), std::move(parent), node);
}

class OdfElement : public virtual common::Element,
                   public std::enable_shared_from_this<OdfElement> {
public:
  OdfElement(std::shared_ptr<const OpenDocument> root,
             std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : m_root{std::move(root)}, m_parent{std::move(parent)}, m_node{node} {}

  std::shared_ptr<const common::Element> parent() const override {
    return m_parent;
  }

  std::shared_ptr<const common::Element> firstChild() const override {
    return firstChildImpl(m_root, shared_from_this(), m_node);
  }

  std::shared_ptr<const common::Element> previousSibling() const override {
    return previousSiblingImpl(m_root, m_parent, m_node);
  }

  std::shared_ptr<const common::Element> nextSibling() const override {
    return nextSiblingImpl(m_root, m_parent, m_node);
  }

  ParagraphProperties paragraphProperties() const {
    if (auto styleAttr = m_node.attribute("text:style-name"); styleAttr) {
      auto style = m_root->m_styles.style(styleAttr.value());
      if (style)
        return style->resolve().toParagraphProperties();
      // TODO log
    }
    return {};
  }

  TextProperties textProperties() const {
    if (auto styleAttr = m_node.attribute("text:style-name"); styleAttr) {
      auto style = m_root->m_styles.style(styleAttr.value());
      if (style)
        return style->resolve().toTextProperties();
      // TODO log
    }
    return {};
  }

protected:
  const std::shared_ptr<const OpenDocument> m_root;
  const std::shared_ptr<const OdfElement> m_parent;
  const pugi::xml_node m_node;
};

class OdfPrimitive final : public OdfElement {
public:
  OdfPrimitive(std::shared_ptr<const OpenDocument> root,
               std::shared_ptr<const OdfElement> parent, pugi::xml_node node,
               const ElementType type)
      : OdfElement(std::move(root), std::move(parent), node), m_type{type} {}

  ElementType type() const final { return m_type; }

private:
  const ElementType m_type;
};

class OdfTextElement final : public OdfElement, public common::TextElement {
public:
  OdfTextElement(std::shared_ptr<const OpenDocument> root,
                 std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(root), std::move(parent), node) {}

  std::string text() const override {
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

class OdfParagraph final : public OdfElement, public common::Paragraph {
public:
  OdfParagraph(std::shared_ptr<const OpenDocument> root,
               std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(root), std::move(parent), node) {}

  ParagraphProperties paragraphProperties() const final {
    return OdfElement::paragraphProperties();
  }

  TextProperties textProperties() const final {
    return OdfElement::textProperties();
  }
};

class OdfSpan final : public OdfElement, public common::Span {
public:
  OdfSpan(std::shared_ptr<const OpenDocument> root,
          std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(root), std::move(parent), node) {}

  TextProperties textProperties() const final {
    return OdfElement::textProperties();
  }
};

class OdfLink final : public OdfElement, public common::Link {
public:
  OdfLink(std::shared_ptr<const OpenDocument> root,
          std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(root), std::move(parent), node) {}

  TextProperties textProperties() const final {
    return OdfElement::textProperties();
  }

  std::string href() const final {
    return m_node.attribute("xlink:href").value();
  }
};

class OdfTableOfContent final : public OdfElement {
public:
  OdfTableOfContent(std::shared_ptr<const OpenDocument> root,
                    std::shared_ptr<const OdfElement> parent,
                    pugi::xml_node node)
      : OdfElement(std::move(root), std::move(parent), node) {}

  ElementType type() const final { return ElementType::UNKNOWN; }

  std::shared_ptr<const Element> firstChild() const override {
    return firstChildImpl(m_root, shared_from_this(),
                          m_node.child("text:index-body"));
  }
};

class OdfBookmark final : public OdfElement, public common::Bookmark {
public:
  OdfBookmark(std::shared_ptr<const OpenDocument> root,
              std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(root), std::move(parent), node) {}

  std::string name() const final {
    return m_node.attribute("text:name").value();
  }
};

class OdfListItem final : public OdfElement, public common::ListItem {
public:
  OdfListItem(std::shared_ptr<const OpenDocument> root,
              std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(root), std::move(parent), node) {}

  std::shared_ptr<const common::Element> previousSibling() const final {
    return knownElementImpl<OdfListItem>(m_root, shared_from_this(), m_node.previous_sibling("text:list-item"));
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
    return knownElementImpl<OdfListItem>(m_root, shared_from_this(), m_node.next_sibling("text:list-item"));
  }
};

class OdfList final : public OdfElement, public common::List {
public:
  OdfList(std::shared_ptr<const OpenDocument> root,
          std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(root), std::move(parent), node) {}

  std::shared_ptr<const common::Element> firstChild() const final {
    return knownElementImpl<OdfListItem>(m_root, shared_from_this(), m_node.child("text:list-item"));
  }
};

class OdfTableColumn final : public OdfElement, public common::TableColumn {
public:
  OdfTableColumn(std::shared_ptr<const OpenDocument> root,
                 std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(root), std::move(parent), node) {}

  std::shared_ptr<const common::Element> firstChild() const final {
    return {};
  }

  std::shared_ptr<const common::Element> previousSibling() const final {
    return knownElementImpl<OdfListItem>(m_root, shared_from_this(), m_node.previous_sibling("table:table-column"));
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
    return knownElementImpl<OdfListItem>(m_root, shared_from_this(), m_node.next_sibling("table:table-column"));
  }
};

class OdfTableCell final : public OdfElement, public common::TableCell {
public:
  OdfTableCell(std::shared_ptr<const OpenDocument> root,
               std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(root), std::move(parent), node) {}

  std::shared_ptr<const common::Element> previousSibling() const final {
      return knownElementImpl<OdfTableCell>(m_root, shared_from_this(), m_node.previous_sibling("table:table-cell"));
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
      return knownElementImpl<OdfTableCell>(m_root, shared_from_this(), m_node.next_sibling("table:table-cell"));
  }

  std::uint32_t rowSpan() const final {
    return m_node.attribute("table:number-rows-spanned").as_uint(1);
  }

  std::uint32_t columnSpan() const final {
    return m_node.attribute("table:number-columns-spanned").as_uint(1);
  }
};

class OdfTableRow final : public OdfElement, public common::TableRow {
public:
  OdfTableRow(std::shared_ptr<const OpenDocument> root,
              std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(root), std::move(parent), node) {}

  std::shared_ptr<const common::Element> firstChild() const final {
    return {};
  }

  std::shared_ptr<const common::Element> previousSibling() const final {
    return knownElementImpl<OdfTableRow>(m_root, shared_from_this(), m_node.previous_sibling("table:table-row"));
  }

  std::shared_ptr<const common::Element> nextSibling() const final {
    return knownElementImpl<OdfTableRow>(m_root, shared_from_this(), m_node.next_sibling("table:table-row"));
  }

  std::shared_ptr<const common::TableCell> firstCell() const final {
    return knownElementImpl<OdfTableCell>(m_root, shared_from_this(), m_node.child("table:table-cell"));
  }
};

class OdfTable final : public OdfElement, public common::Table {
public:
  OdfTable(std::shared_ptr<const OpenDocument> root,
           std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(root), std::move(parent), node) {}

  std::uint32_t rowCount() const final {
    return 0; // TODO
  }

  std::uint32_t columnCount() const final {
    return 0; // TODO
  }

  std::shared_ptr<const common::Element> firstChild() const final {
    return {};
  }

  std::shared_ptr<const common::TableColumn> firstColumn() const final {
    return knownElementImpl<OdfTableColumn>(m_root, shared_from_this(), m_node.child("table:table-column"));
  }

  std::shared_ptr<const common::TableRow> firstRow() const final {
    return knownElementImpl<OdfTableRow>(m_root, shared_from_this(), m_node.child("table:table-row"));
  }
};

std::shared_ptr<common::Element>
convert(std::shared_ptr<const OpenDocument> root,
        std::shared_ptr<const OdfElement> parent, pugi::xml_node node) {
  if (node.type() == pugi::node_pcdata) {
    return std::make_shared<OdfTextElement>(std::move(root), std::move(parent),
                                            node);
  } else if (node.type() == pugi::node_element) {
    const std::string element = node.name();

    if (element == "text:p" || element == "text:h")
      return std::make_shared<OdfParagraph>(std::move(root), std::move(parent),
                                            node);
    else if (element == "text:span")
      return std::make_shared<OdfSpan>(std::move(root), std::move(parent),
                                       node);
    else if (element == "text:s" || element == "text:tab")
      return std::make_shared<OdfTextElement>(std::move(root),
                                              std::move(parent), node);
    else if (element == "text:line-break")
      return std::make_shared<OdfPrimitive>(std::move(root), std::move(parent),
                                            node, ElementType::LINE_BREAK);
    else if (element == "text:a")
      return std::make_shared<OdfLink>(std::move(root), std::move(parent),
                                       node);
    else if (element == "text:table-of-content")
      return std::make_shared<OdfTableOfContent>(std::move(root),
                                                 std::move(parent), node);
    else if (element == "text:bookmark" || element == "text:bookmark-start")
      return std::make_shared<OdfBookmark>(std::move(root), std::move(parent),
                                           node);
    else if (element == "text:list")
      return std::make_shared<OdfList>(std::move(root), std::move(parent),
                                       node);
    else if (element == "table:table")
      return std::make_shared<OdfTable>(std::move(root), std::move(parent),
                                        node);

    // else if (element == "draw:frame" || element == "draw:custom-shape")
    //  FrameTranslator(in, out, context);
    // else if (element == "draw:image")
    //  ImageTranslator(in, out, context);

    // TODO this should be removed at some point
    return std::make_shared<OdfPrimitive>(std::move(root), std::move(parent),
                                          node, ElementType::UNKNOWN);
  }

  return nullptr;
}

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

std::shared_ptr<common::Element>
firstChildImpl(std::shared_ptr<const OpenDocument> root,
               std::shared_ptr<const OdfElement> parent, pugi::xml_node node) {
  for (auto &&c : node) {
    if (isSkipper(c))
      continue;
    return convert(std::move(root), std::move(parent), c);
  }
  return nullptr;
}

std::shared_ptr<common::Element>
previousSiblingImpl(std::shared_ptr<const OpenDocument> root,
                    std::shared_ptr<const OdfElement> parent,
                    pugi::xml_node node) {
  for (auto &&s = node.previous_sibling(); s; s = node.previous_sibling()) {
    if (isSkipper(s))
      continue;
    return convert(std::move(root), std::move(parent), s);
  }
  return nullptr;
}

std::shared_ptr<common::Element>
nextSiblingImpl(std::shared_ptr<const OpenDocument> root,
                std::shared_ptr<const OdfElement> parent, pugi::xml_node node) {
  for (auto &&s = node.next_sibling(); s; s = s.next_sibling()) {
    if (isSkipper(s))
      continue;
    return convert(std::move(root), std::move(parent), s);
  }
  return nullptr;
}
} // namespace

OpenDocument::OpenDocument(std::shared_ptr<access::ReadStorage> storage)
    : m_storage{std::move(storage)} {
  m_contentXml = common::XmlUtil::parse(*m_storage, "content.xml");

  if (!m_storage->isFile("meta.xml")) {
    auto meta = common::XmlUtil::parse(*m_storage, "meta.xml");
    m_document_meta = parseDocumentMeta(&meta, m_contentXml);
  } else {
    m_document_meta = parseDocumentMeta(nullptr, m_contentXml);
  }

  if (m_storage->isFile("styles.xml"))
    m_stylesXml = common::XmlUtil::parse(*m_storage, "styles.xml");
  m_styles =
      Styles(m_stylesXml.document_element(), m_contentXml.document_element());
}

bool OpenDocument::editable() const noexcept { return true; }

bool OpenDocument::savable(bool encrypted) const noexcept { return !encrypted; }

DocumentType OpenDocument::documentType() const noexcept {
  return m_document_meta.documentType;
}

DocumentMeta OpenDocument::documentMeta() const noexcept {
  return m_document_meta;
}

void OpenDocument::save(const access::Path &path) const {
  // TODO throw if not savable
  // TODO this would decrypt/inflate and encrypt/deflate again
  access::ZipWriter writer(path);

  // `mimetype` has to be the first file and uncompressed
  if (m_storage->isFile("mimetype")) {
    const auto in = m_storage->read("mimetype");
    const auto out = writer.write("mimetype", 0);
    access::StreamUtil::pipe(*in, *out);
  }

  m_storage->visit([&](const auto &p) {
    std::cout << p.string() << std::endl;
    if (p == "mimetype")
      return;
    if (m_storage->isDirectory(p)) {
      writer.createDirectory(p);
      return;
    }
    const auto in = m_storage->read(p);
    const auto out = writer.write(p);
    if (p == "content.xml") {
      m_contentXml.print(*out);
      return;
    }
    access::StreamUtil::pipe(*in, *out);
  });
}

void OpenDocument::save(const access::Path &path,
                        const std::string &password) const {
  // TODO throw if not savable
}

OpenDocumentText::OpenDocumentText(std::shared_ptr<access::ReadStorage> storage)
    : OpenDocument(std::move(storage)) {}

PageProperties OpenDocumentText::pageProperties() const {
  return m_styles.defaultPageProperties();
}

ElementSiblingRange OpenDocumentText::content() const {
  const pugi::xml_node body =
      m_contentXml.document_element().child("office:body").child("office:text");
  return ElementSiblingRange(
      Element(firstChildImpl(shared_from_this(), nullptr, body)));
}

OpenDocumentPresentation::OpenDocumentPresentation(
    std::shared_ptr<access::ReadStorage> storage)
    : OpenDocument(std::move(storage)) {}

ElementSiblingRange
OpenDocumentPresentation::slideContent(const std::uint32_t index) const {
  // TODO throw if out of bounds
  const pugi::xml_node body = m_contentXml.document_element()
                                  .child("office:body")
                                  .child("office:presentation");
  std::uint32_t i = 0;
  for (auto &&page : body.children("draw:page")) {
    if (i == index)
      return ElementSiblingRange(
          Element(firstChildImpl(shared_from_this(), nullptr, page)));
  }
  return {};
}

OpenDocumentSpreadsheet::OpenDocumentSpreadsheet(
    std::shared_ptr<access::ReadStorage> storage)
    : OpenDocument(std::move(storage)) {}

TableElement
OpenDocumentSpreadsheet::sheetTable(const std::uint32_t index) const {
  // TODO
}

OpenDocumentGraphics::OpenDocumentGraphics(
    std::shared_ptr<access::ReadStorage> storage)
    : OpenDocument(std::move(storage)) {}

ElementSiblingRange
OpenDocumentGraphics::pageContent(const std::uint32_t index) const {
  // TODO throw if out of bounds
  const pugi::xml_node body = m_contentXml.document_element()
                                  .child("office:body")
                                  .child("office:drawing");
  std::uint32_t i = 0;
  for (auto &&page : body.children("draw:page")) {
    if (i == index)
      return ElementSiblingRange(
          Element(firstChildImpl(shared_from_this(), nullptr, page)));
  }
  return {};
}

} // namespace odr::odf
