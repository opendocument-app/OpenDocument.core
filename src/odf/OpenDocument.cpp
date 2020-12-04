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

class OdfElement : public virtual common::Element,
                   public std::enable_shared_from_this<OdfElement> {
public:
  OdfElement(std::shared_ptr<const OpenDocument> root,
             std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : m_root{std::move(root)}, m_parent{std::move(parent)}, m_node{node} {}

  std::shared_ptr<const Element> parent() const override { return m_parent; }

  std::shared_ptr<const Element> firstChild() const override {
    return firstChildImpl(m_root, shared_from_this(), m_node);
  }

  std::shared_ptr<const Element> previousSibling() const override {
    return previousSiblingImpl(m_root, m_parent, m_node);
  }

  std::shared_ptr<const Element> nextSibling() const override {
    return nextSiblingImpl(m_root, m_parent, m_node);
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

class OdfTextElement : public OdfElement, public common::TextElement {
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

class OdfParagraph : public OdfElement, public common::Paragraph {
public:
  OdfParagraph(std::shared_ptr<const OpenDocument> root,
               std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(root), std::move(parent), node) {}

  ParagraphProperties paragraphProperties() const final {
    if (auto style = m_node.attribute("text:style-name"); style)
      return m_root->m_style.paragraphProperties(style.value());
    return {};
  }

  TextProperties textProperties() const final {
    if (auto style = m_node.attribute("text:style-name"); style)
      return m_root->m_style.textProperties(style.value());
    return {};
  }
};

class OdfSpan : public OdfElement, public common::Span {
public:
  OdfSpan(std::shared_ptr<const OpenDocument> root,
          std::shared_ptr<const OdfElement> parent, pugi::xml_node node)
      : OdfElement(std::move(root), std::move(parent), node) {}

  TextProperties textProperties() const final {
    if (auto style = m_node.attribute("text:style-name"); style)
      return m_root->m_style.textProperties(style.value());
    return {}; // TODO optional?
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
    // else if (element == "text:a")
    //  LinkTranslator(in, out, context);
    // else if (element == "text:bookmark" || element == "text:bookmark-start")
    //  BookmarkTranslator(in, out, context);
    // else if (element == "draw:frame" || element == "draw:custom-shape")
    //  FrameTranslator(in, out, context);
    // else if (element == "draw:image")
    //  ImageTranslator(in, out, context);
    // else if (element == "table:table")
    //  TableTranslator(in, out, context);

    return std::make_shared<OdfPrimitive>(std::move(root), std::move(parent),
                                          node, ElementType::UNKNOWN);
  }

  return nullptr;
}

bool isSkipper(pugi::xml_node node) {
  const std::string element = node.name();

  if (element == "office:forms")
    return true;
  if (element == "text:sequence-decls")
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
  for (auto &&s = node.next_sibling(); s; s = node.next_sibling()) {
    if (isSkipper(s))
      continue;
    return convert(std::move(root), std::move(parent), s);
  }
  return nullptr;
}
} // namespace

OpenDocument::OpenDocument(std::shared_ptr<access::ReadStorage> storage)
    : m_storage{std::move(storage)} {
  m_content = common::XmlUtil::parse(*m_storage, "content.xml");

  if (!m_storage->isFile("meta.xml")) {
    auto meta = common::XmlUtil::parse(*m_storage, "meta.xml");
    m_document_meta = parseDocumentMeta(&meta, m_content);
  } else {
    m_document_meta = parseDocumentMeta(nullptr, m_content);
  }

  pugi::xml_document styles;
  if (m_storage->isFile("styles.xml"))
    styles = common::XmlUtil::parse(*m_storage, "styles.xml");
  m_style = Style(std::move(styles), m_content.document_element());
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
      m_content.print(*out);
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
  return m_style.defaultPageProperties();
}

ElementSiblingRange OpenDocumentText::content() const {
  const pugi::xml_node body = m_content.child("office:document-content")
                                  .child("office:body")
                                  .child("office:text");
  return ElementSiblingRange(Element(firstChildImpl(shared_from_this(), nullptr, body)));
}

OpenDocumentPresentation::OpenDocumentPresentation(
    std::shared_ptr<access::ReadStorage> storage)
    : OpenDocument(std::move(storage)) {}

ElementSiblingRange
OpenDocumentPresentation::slideContent(std::uint32_t index) const {
  // TODO
}

OpenDocumentSpreadsheet::OpenDocumentSpreadsheet(
    std::shared_ptr<access::ReadStorage> storage)
    : OpenDocument(std::move(storage)) {}

TableElement OpenDocumentSpreadsheet::sheetTable(std::uint32_t index) const {
  // TODO
}

OpenDocumentGraphics::OpenDocumentGraphics(
    std::shared_ptr<access::ReadStorage> storage)
    : OpenDocument(std::move(storage)) {}

ElementSiblingRange
OpenDocumentGraphics::pageContent(std::uint32_t index) const {
  // TODO
}

} // namespace odr::odf
