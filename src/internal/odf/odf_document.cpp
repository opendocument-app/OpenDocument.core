#include <fstream>
#include <internal/abstract/filesystem.h>
#include <internal/common/file.h>
#include <internal/common/path.h>
#include <internal/odf/odf_cursor.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_style.h>
#include <internal/util/xml_util.h>
#include <internal/zip/zip_archive.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <sstream>
#include <utility>

namespace odr::internal::abstract {
class DocumentCursor;
} // namespace odr::internal::abstract

namespace odr::internal::odf {

Document::Document(const FileType file_type, const DocumentType document_type,
                   std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_file_type{file_type}, m_document_type{document_type},
      m_filesystem{std::move(filesystem)} {
  m_content_xml = util::xml::parse(*m_filesystem, "content.xml");

  if (m_filesystem->exists("styles.xml")) {
    m_styles_xml = util::xml::parse(*m_filesystem, "styles.xml");
  }

  m_style_registry = StyleRegistry(m_content_xml.document_element(),
                                   m_styles_xml.document_element());
}

bool Document::editable() const noexcept { return true; }

bool Document::savable(const bool encrypted) const noexcept {
  return !encrypted;
}

void Document::save(const common::Path &path) const {
  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipArchive archive;

  // `mimetype` has to be the first file and uncompressed
  if (m_filesystem->is_file("mimetype")) {
    archive.insert_file(std::end(archive), "mimetype",
                        m_filesystem->open("mimetype"), 0);
  }

  for (auto walker = m_filesystem->file_walker(""); !walker->end();
       walker->next()) {
    auto p = walker->path();
    if (p == "mimetype") {
      continue;
    }
    if (m_filesystem->is_directory(p)) {
      archive.insert_directory(std::end(archive), p);
      continue;
    }
    if (p == "content.xml") {
      // TODO stream
      std::stringstream out;
      m_content_xml.print(out, "", pugi::format_raw);
      auto tmp = std::make_shared<common::MemoryFile>(out.str());
      archive.insert_file(std::end(archive), p, tmp);
      continue;
    }
    if (p == "META-INF/manifest.xml") {
      // TODO
      auto manifest = util::xml::parse(*m_filesystem, "META-INF/manifest.xml");

      for (auto &&node : manifest.select_nodes("//manifest:encryption-data")) {
        node.node().parent().remove_child(node.node());
      }

      std::stringstream out;
      manifest.print(out, "", pugi::format_raw);
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
  // TODO throw if not savable
  throw UnsupportedOperation();
}

FileType Document::file_type() const noexcept { return m_file_type; }

DocumentType Document::document_type() const noexcept {
  return m_document_type;
}

std::shared_ptr<abstract::ReadableFilesystem> Document::files() const noexcept {
  return m_filesystem;
}

std::unique_ptr<abstract::DocumentCursor> Document::root_element() const {
  return std::make_unique<DocumentCursor>(
      this,
      m_content_xml.document_element().child("office:body").first_child());
}

} // namespace odr::internal::odf
