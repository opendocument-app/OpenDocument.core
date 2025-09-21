#include <odr/internal/odf/odf_document.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/odf/odf_parser.hpp>
#include <odr/internal/util/file_util.hpp>
#include <odr/internal/util/xml_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <fstream>
#include <sstream>

namespace odr::internal::odf {

Document::Document(const FileType file_type, const DocumentType document_type,
                   std::shared_ptr<abstract::ReadableFilesystem> files)
    : TemplateDocument(file_type, document_type, std::move(files)) {
  m_content_xml = util::xml::parse(*m_filesystem, AbsPath("/content.xml"));

  if (m_filesystem->exists(AbsPath("/styles.xml"))) {
    m_styles_xml = util::xml::parse(*m_filesystem, AbsPath("/styles.xml"));
  }

  m_root_element = parse_tree(
      *this,
      m_content_xml.document_element().child("office:body").first_child());

  m_style_registry = StyleRegistry(*this, m_content_xml.document_element(),
                                   m_styles_xml.document_element());
}

bool Document::is_editable() const noexcept { return true; }

bool Document::is_savable(const bool encrypted) const noexcept {
  return !encrypted;
}

void Document::save(const Path &path) const {
  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipArchive archive;

  // `mimetype` has to be the first file and uncompressed
  if (m_filesystem->is_file(AbsPath("/mimetype"))) {
    archive.insert_file(std::end(archive), RelPath("mimetype"),
                        m_filesystem->open(AbsPath("/mimetype")), 0);
  }

  for (auto walker = m_filesystem->file_walker(AbsPath("/")); !walker->end();
       walker->next()) {
    const AbsPath &abs_path = walker->path();
    RelPath rel_path = abs_path.rebase(AbsPath("/"));
    if (abs_path == Path("/mimetype")) {
      continue;
    }
    if (walker->is_directory()) {
      archive.insert_directory(std::end(archive), rel_path);
      continue;
    }
    if (abs_path == Path("/content.xml")) {
      // TODO stream
      std::stringstream out;
      m_content_xml.print(out, "", pugi::format_raw);
      auto tmp = std::make_shared<MemoryFile>(out.str());
      archive.insert_file(std::end(archive), rel_path, tmp);
      continue;
    }
    if (abs_path == Path("/META-INF/manifest.xml")) {
      // TODO
      auto manifest =
          util::xml::parse(*m_filesystem, AbsPath("/META-INF/manifest.xml"));

      for (auto &&node : manifest.select_nodes("//manifest:encryption-data")) {
        node.node().parent().remove_child(node.node());
      }

      std::stringstream out;
      manifest.print(out, "", pugi::format_raw);
      auto tmp = std::make_shared<MemoryFile>(out.str());
      archive.insert_file(std::end(archive), rel_path, tmp);

      continue;
    }
    archive.insert_file(std::end(archive), rel_path,
                        m_filesystem->open(abs_path));
  }

  std::ofstream ostream = util::file::create(path.string());
  archive.save(ostream);
}

void Document::save(const Path & /*path*/, const char * /*password*/) const {
  // TODO throw if not savable
  throw UnsupportedOperation();
}

} // namespace odr::internal::odf
