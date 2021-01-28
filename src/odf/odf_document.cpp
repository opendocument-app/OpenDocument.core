#include <abstract/filesystem.h>
#include <common/path.h>
#include <odf/odf_crypto.h>
#include <odf/odf_document.h>
#include <odf/odf_elements.h>
#include <odr/document.h>
#include <odr/document_elements.h>
#include <util/stream_util.h>
#include <util/xml_util.h>

namespace odr::odf {

OpenDocument::OpenDocument(std::shared_ptr<abstract::ReadableFilesystem> files)
    : m_files{std::move(files)} {
  m_content_xml = util::xml::parse(*m_files, "content.xml");

  if (m_files->exists("meta.xml")) {
    auto meta = util::xml::parse(*m_files, "meta.xml");
    m_document_meta = parse_document_meta(&meta, m_content_xml);
  } else {
    m_document_meta = parse_document_meta(nullptr, m_content_xml);
  }

  if (m_files->exists("styles.xml")) {
    m_styles_xml = util::xml::parse(*m_files, "styles.xml");
  }
  m_styles =
      Styles(m_styles_xml.document_element(), m_content_xml.document_element());
}

bool OpenDocument::editable() const noexcept { return true; }

bool OpenDocument::savable(bool encrypted) const noexcept { return !encrypted; }

DocumentType OpenDocument::document_type() const noexcept {
  return m_document_meta.document_type;
}

DocumentMeta OpenDocument::document_meta() const noexcept {
  return m_document_meta;
}

std::shared_ptr<abstract::ReadableFilesystem>
OpenDocument::filesystem() const noexcept {
  return m_files;
}

const Styles &OpenDocument::styles() const noexcept { return m_styles; }

void OpenDocument::save(const common::Path &path) const {
  // TODO

  /*
  // TODO throw if not savable
  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipWriter writer(path);

  // `mimetype` has to be the first file and uncompressed
  if (m_files->is_file("mimetype")) {
    const auto in = m_files->read("mimetype");
    const auto out = writer.write("mimetype", 0);
    util::stream::pipe(*in, *out);
  }

  m_files->visit([&](const auto &p) {
    std::cout << p.string() << std::endl;
    if (p == "mimetype") {
      return;
    }
    if (m_files->is_directory(p)) {
      writer.createDirectory(p);
      return;
    }
    const auto in = m_files->read(p);
    const auto out = writer.write(p);
    if (p == "content.xml") {
      m_content_xml.print(*out);
      return;
    }
    util::stream::pipe(*in, *out);
  });
   */
}

void OpenDocument::save(const common::Path &path,
                        const std::string &password) const {
  // TODO throw if not savable
}

OpenDocumentText::OpenDocumentText(
    std::shared_ptr<abstract::ReadableFilesystem> files)
    : OpenDocument(std::move(files)) {}

std::shared_ptr<const abstract::Element> OpenDocumentText::root() const {
  const pugi::xml_node body = m_content_xml.document_element()
                                  .child("office:body")
                                  .child("office:text");
  return factorize_root(shared_from_this(), body);
}

std::shared_ptr<abstract::PageStyle> OpenDocumentText::page_style() const {
  return m_styles.default_page_style();
}

OpenDocumentPresentation::OpenDocumentPresentation(
    std::shared_ptr<abstract::ReadableFilesystem> files)
    : OpenDocument(std::move(files)) {}

std::uint32_t OpenDocumentPresentation::slide_count() const {
  return 0; // TODO
}

std::shared_ptr<const abstract::Element>
OpenDocumentPresentation::root() const {
  const pugi::xml_node body = m_content_xml.document_element()
                                  .child("office:body")
                                  .child("office:presentation");
  return factorize_root(shared_from_this(), body);
}

std::shared_ptr<const abstract::Slide>
OpenDocumentPresentation::first_slide() const {
  return std::dynamic_pointer_cast<const abstract::Slide>(
      root()->first_child());
}

OpenDocumentSpreadsheet::OpenDocumentSpreadsheet(
    std::shared_ptr<abstract::ReadableFilesystem> files)
    : OpenDocument(std::move(files)) {}

std::uint32_t OpenDocumentSpreadsheet::sheet_count() const {
  return 0; // TODO
}

std::shared_ptr<const abstract::Element> OpenDocumentSpreadsheet::root() const {
  const pugi::xml_node body = m_content_xml.document_element()
                                  .child("office:body")
                                  .child("office:spreadsheet");
  return factorize_root(shared_from_this(), body);
}

std::shared_ptr<const abstract::Sheet>
OpenDocumentSpreadsheet::first_sheet() const {
  return std::dynamic_pointer_cast<const abstract::Sheet>(
      root()->first_child());
}

OpenDocumentDrawing::OpenDocumentDrawing(
    std::shared_ptr<abstract::ReadableFilesystem> files)
    : OpenDocument(std::move(files)) {}

std::uint32_t OpenDocumentDrawing::page_count() const {
  return 0; // TODO
}

std::shared_ptr<const abstract::Element> OpenDocumentDrawing::root() const {
  const pugi::xml_node body = m_content_xml.document_element()
                                  .child("office:body")
                                  .child("office:drawing");
  return factorize_root(shared_from_this(), body);
}

std::shared_ptr<const abstract::Page> OpenDocumentDrawing::first_page() const {
  return std::dynamic_pointer_cast<const abstract::Page>(root()->first_child());
}

} // namespace odr::odf
