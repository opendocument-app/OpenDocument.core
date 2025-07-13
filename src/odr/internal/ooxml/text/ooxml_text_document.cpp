#include <odr/internal/ooxml/text/ooxml_text_document.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/text/ooxml_text_parser.hpp>
#include <odr/internal/util/file_util.hpp>
#include <odr/internal/util/xml_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <fstream>
#include <sstream>

namespace odr::internal::ooxml::text {

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : TemplateDocument<Element>(FileType::office_open_xml_document,
                                DocumentType::text, std::move(filesystem)) {
  m_document_xml = util::xml::parse(*m_filesystem, Path("/word/document.xml"));
  m_styles_xml = util::xml::parse(*m_filesystem, Path("/word/styles.xml"));

  m_document_relations =
      parse_relationships(*m_filesystem, Path("/word/document.xml"));

  m_root_element =
      parse_tree(*this, m_document_xml.document_element().child("w:body"));

  m_style_registry = StyleRegistry(m_styles_xml.document_element());
}

bool Document::is_editable() const noexcept { return false; }

bool Document::is_savable(const bool encrypted) const noexcept {
  return !encrypted;
}

void Document::save(const Path &path) const {
  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipArchive archive;

  for (auto walker = m_filesystem->file_walker(Path("/")); !walker->end();
       walker->next()) {
    auto p = walker->path();
    if (m_filesystem->is_directory(p)) {
      archive.insert_directory(std::end(archive), p);
      continue;
    }
    if (p == Path("/word/document.xml")) {
      // TODO stream
      std::stringstream out;
      m_document_xml.print(out, "", pugi::format_raw);
      auto tmp = std::make_shared<MemoryFile>(out.str());
      archive.insert_file(std::end(archive), p, tmp);
      continue;
    }
    archive.insert_file(std::end(archive), p, m_filesystem->open(p));
  }

  std::ofstream ostream = util::file::create(path.string());
  archive.save(ostream);
}

void Document::save(const Path & /*path*/, const char * /*password*/) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::ooxml::text
