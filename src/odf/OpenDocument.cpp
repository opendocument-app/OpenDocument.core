#include <access/StreamUtil.h>
#include <access/ZipStorage.h>
#include <common/XmlUtil.h>
#include <odf/Crypto.h>
#include <odf/OpenDocument.h>
#include <odr/Document.h>
#include <odr/DocumentElements.h>
#include <odf/Elements.h>

namespace odr::odf {

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

const Styles &OpenDocument::styles() const noexcept {
  return m_styles;
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

ElementRange OpenDocumentText::content() const {
  const pugi::xml_node body =
      m_contentXml.document_element().child("office:body").child("office:text");
  return ElementRange(
      Element(factorizeFirstChild(shared_from_this(), nullptr, body)));
}

OpenDocumentPresentation::OpenDocumentPresentation(
    std::shared_ptr<access::ReadStorage> storage)
    : OpenDocument(std::move(storage)) {}

ElementRange
OpenDocumentPresentation::slideContent(const std::uint32_t index) const {
  // TODO throw if out of bounds
  const pugi::xml_node body = m_contentXml.document_element()
                                  .child("office:body")
                                  .child("office:presentation");
  std::uint32_t i = 0;
  for (auto &&page : body.children("draw:page")) {
    if (i == index)
      return ElementRange(
          Element(factorizeFirstChild(shared_from_this(), nullptr, page)));
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

ElementRange
OpenDocumentGraphics::pageContent(const std::uint32_t index) const {
  // TODO throw if out of bounds
  const pugi::xml_node body = m_contentXml.document_element()
                                  .child("office:body")
                                  .child("office:drawing");
  std::uint32_t i = 0;
  for (auto &&page : body.children("draw:page")) {
    if (i == index)
      return ElementRange(
          Element(factorizeFirstChild(shared_from_this(), nullptr, page)));
  }
  return {};
}

} // namespace odr::odf
