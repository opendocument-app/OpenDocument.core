#include <access/StreamUtil.h>
#include <access/ZipStorage.h>
#include <common/XmlUtil.h>
#include <odf/Crypto.h>
#include <odf/Elements.h>
#include <odf/OpenDocument.h>
#include <odr/Document.h>
#include <odr/DocumentElements.h>

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

const Styles &OpenDocument::styles() const noexcept { return m_styles; }

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

std::uint32_t OpenDocumentPresentation::slideCount() const {
  return slides().size();
}

std::vector<Slide> OpenDocumentPresentation::slides() const {
  std::vector<Slide> result;

  const pugi::xml_node body = m_contentXml.document_element()
                                  .child("office:body")
                                  .child("office:presentation");
  for (auto &&xml : body.children("draw:page")) {
    Slide slide;
    slide.name = xml.attribute("draw:name").value();
    slide.notes = ""; // TODO
    slide.pageProperties = m_styles.masterPageProperties(
        xml.attribute("draw:master-page-name").value());
    slide.content = ElementRange(
        Element(factorizeFirstChild(shared_from_this(), nullptr, xml)));
    result.push_back(slide);
  }

  return result;
}

OpenDocumentSpreadsheet::OpenDocumentSpreadsheet(
    std::shared_ptr<access::ReadStorage> storage)
    : OpenDocument(std::move(storage)) {}

std::uint32_t OpenDocumentSpreadsheet::sheetCount() const {
  return sheets().size();
}

std::vector<Sheet> OpenDocumentSpreadsheet::sheets() const {
  std::vector<Sheet> result;

  const pugi::xml_node body = m_contentXml.document_element()
                                  .child("office:body")
                                  .child("office:presentation");
  for (auto &&xml : body.children("draw:page")) {
    Sheet sheet;
    sheet.name = xml.attribute("draw:name").value();
    sheet.rowCount = 0;    // TODO
    sheet.columnCount = 0; // TODO
    sheet.table =
        Element(factorizeFirstChild(shared_from_this(), nullptr, xml)).table();
    result.push_back(sheet);
  }

  return result;
}

OpenDocumentGraphics::OpenDocumentGraphics(
    std::shared_ptr<access::ReadStorage> storage)
    : OpenDocument(std::move(storage)) {}

std::uint32_t OpenDocumentGraphics::pageCount() const { return pages().size(); }

std::vector<Page> OpenDocumentGraphics::pages() const {
  std::vector<Page> result;

  const pugi::xml_node body = m_contentXml.document_element()
                                  .child("office:body")
                                  .child("office:drawing");
  for (auto &&xml : body.children("draw:page")) {
    Page page;
    page.name = xml.attribute("draw:name").value();
    page.pageProperties = {}; // TODO
    page.content = ElementRange(
        Element(factorizeFirstChild(shared_from_this(), nullptr, xml)));
    result.push_back(page);
  }

  return result;
}

} // namespace odr::odf
