#ifndef ODR_INTERNAL_COMMON_DOCUMENT_H
#define ODR_INTERNAL_COMMON_DOCUMENT_H

#include <odr/internal/abstract/document.hpp>

#include <vector>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::common {
class Element;

class Document : public abstract::Document {
public:
  Document(FileType file_type, DocumentType document_type,
           std::shared_ptr<abstract::ReadableFilesystem> files);

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] DocumentType document_type() const noexcept final;

  [[nodiscard]] std::shared_ptr<abstract::ReadableFilesystem>
  files() const noexcept final;

protected:
  FileType m_file_type;
  DocumentType m_document_type;

  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  friend class Element;
};

template <typename element_t> class TemplateDocument : public Document {
public:
  TemplateDocument(FileType file_type, DocumentType document_type,
                   std::shared_ptr<abstract::ReadableFilesystem> filesystem)
      : Document(file_type, document_type, std::move(filesystem)) {}

  [[nodiscard]] abstract::Element *root_element() const final {
    return m_root_element;
  }

  void register_element_(std::unique_ptr<element_t> element) {
    m_elements.push_back(std::move(element));
  }

protected:
  std::vector<std::unique_ptr<element_t>> m_elements;
  element_t *m_root_element{};
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_H
