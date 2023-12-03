#include <odr/internal/ooxml/text/ooxml_text_document.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/text/ooxml_text_parser.hpp>
#include <odr/internal/util/xml_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <fstream>
#include <sstream>
#include <utility>

namespace odr::internal::ooxml::text {

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  m_document_xml = util::xml::parse(*m_filesystem, "word/document.xml");
  m_styles_xml = util::xml::parse(*m_filesystem, "word/styles.xml");

  m_document_relations =
      parse_relationships(*m_filesystem, "word/document.xml");

  m_root_element =
      parse_tree(*this, m_document_xml.document_element().child("w:body"));

  m_style_registry = StyleRegistry(m_styles_xml.document_element());
}

bool Document::editable() const noexcept { return false; }

bool Document::savable(const bool encrypted) const noexcept {
  return !encrypted;
}

void Document::save(const common::Path &path) const {
  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipArchive archive;

  for (auto walker = m_filesystem->file_walker(""); !walker->end();
       walker->next()) {
    auto p = walker->path();
    if (m_filesystem->is_directory(p)) {
      archive.insert_directory(std::end(archive), p);
      continue;
    }
    if (p == "word/document.xml") {
      // TODO stream
      std::stringstream out;
      m_document_xml.print(out, "", pugi::format_raw);
      auto tmp = std::make_shared<common::MemoryFile>(out.str());
      archive.insert_file(std::end(archive), p, tmp);
      continue;
    }
    archive.insert_file(std::end(archive), p, m_filesystem->open(p));
  }

  std::ofstream ostream(path.path());
  archive.save(ostream);
}

void Document::save(const common::Path & /*path*/,
                    const char * /*password*/) const {
  throw UnsupportedOperation();
}

FileType Document::file_type() const noexcept {
  return FileType::office_open_xml_document;
}

DocumentType Document::document_type() const noexcept {
  return DocumentType::text;
}

std::shared_ptr<abstract::ReadableFilesystem> Document::files() const noexcept {
  return m_filesystem;
}

abstract::Element *Document::root_element() const { return m_root_element; }

void Document::register_element_(std::unique_ptr<Element> element) {
  m_elements.push_back(std::move(element));
}

} // namespace odr::internal::ooxml::text
