#include <fstream>
#include <internal/abstract/filesystem.h>
#include <internal/common/file.h>
#include <internal/common/path.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_sheet.h>
#include <internal/util/xml_util.h>
#include <internal/zip/zip_archive.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <unordered_map>
#include <utility>

namespace odr::internal::odf {

namespace {
class DocumentCursor final : public abstract::DocumentCursor {
public:
  bool operator==(const abstract::DocumentCursor &rhs) const final {
    return false; // TODO
  }

  bool operator!=(const abstract::DocumentCursor &rhs) const final {
    return true; // TODO
  }

  [[nodiscard]] std::unique_ptr<abstract::DocumentCursor> copy() const final {
    return {}; // TODO
  }

  [[nodiscard]] std::string document_path() const final {
    return ""; // TODO
  }

  [[nodiscard]] ElementType type() const final {
    return ElementType::NONE; // TODO
  }

  bool parent() final {
    return false; // TODO
  }

  bool first_child() final {
    return false; // TODO
  }

  bool previous_sibling() final {
    return false; // TODO
  }

  bool next_sibling() final {
    return false; // TODO
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    return {}; // TODO
  }
};
} // namespace

OpenDocument::OpenDocument(
    const DocumentType document_type,
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_document_type{document_type}, m_filesystem{std::move(filesystem)} {
  m_content_xml = util::xml::parse(*m_filesystem, "content.xml");

  if (m_filesystem->exists("styles.xml")) {
    m_styles_xml = util::xml::parse(*m_filesystem, "styles.xml");
  }

  m_style =
      Style(m_content_xml.document_element(), m_styles_xml.document_element());
}

bool OpenDocument::editable() const noexcept { return true; }

bool OpenDocument::savable(const bool encrypted) const noexcept {
  return !encrypted;
}

void OpenDocument::save(const common::Path &path) const {
  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipArchive archive;

  // `mimetype` has to be the first file and uncompressed
  if (m_filesystem->is_file("mimetype")) {
    archive.insert_file(std::end(archive), "mimetype",
                        m_filesystem->open("mimetype"), 0);
  }

  for (auto walker = m_filesystem->file_walker("/"); !walker->end();
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
      m_content_xml.print(out);
      auto tmp = std::make_shared<common::MemoryFile>(out.str());
      archive.insert_file(std::end(archive), p, tmp);
      continue;
    }
    archive.insert_file(std::end(archive), p, m_filesystem->open(p));
  }

  std::ofstream ostream(path.path());
  archive.save(ostream);
}

void OpenDocument::save(const common::Path & /*path*/,
                        const char * /*password*/) const {
  // TODO throw if not savable
  throw UnsupportedOperation();
}

DocumentType OpenDocument::document_type() const noexcept {
  return m_document_type;
}

std::shared_ptr<abstract::ReadableFilesystem>
OpenDocument::files() const noexcept {
  return m_filesystem;
}

std::unique_ptr<abstract::DocumentCursor> OpenDocument::root_element() const {
  return {}; // TODO
}

} // namespace odr::internal::odf
