#ifndef ODR_INTERNAL_ODF_DOCUMENT_H
#define ODR_INTERNAL_ODF_DOCUMENT_H

#include <odr/file.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/document_element.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/odf/odf_element.hpp>
#include <odr/internal/odf/odf_style.hpp>

#include <memory>

#include <pugixml.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::odf {

class Document : public abstract::Document {
public:
  Document(FileType file_type, DocumentType document_type,
           std::shared_ptr<abstract::ReadableFilesystem> files);

  bool editable() const noexcept final;
  bool savable(bool encrypted) const noexcept final;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const char *password) const final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] DocumentType document_type() const noexcept final;

  [[nodiscard]] std::shared_ptr<abstract::ReadableFilesystem>
  files() const noexcept final;

  [[nodiscard]] abstract::Element *root_element() const final;

protected:
  FileType m_file_type;
  DocumentType m_document_type;

  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  pugi::xml_document m_content_xml;
  pugi::xml_document m_styles_xml;

  std::vector<std::unique_ptr<Element>> m_elements;
  Element *m_root_element{};

  StyleRegistry m_style_registry;

  friend class Element;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_DOCUMENT_H
