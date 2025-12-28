#pragma once

#include <odr/definitions.hpp>
#include <odr/internal/abstract/document.hpp>

#include <memory>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal {

class Document : public abstract::Document {
public:
  Document(FileType file_type, DocumentType document_type,
           std::shared_ptr<abstract::ReadableFilesystem> files);
  ~Document() override;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] DocumentType document_type() const noexcept final;

  [[nodiscard]] std::shared_ptr<abstract::ReadableFilesystem>
  as_filesystem() const noexcept final;

  [[nodiscard]] ElementIdentifier root_element() const override;

  [[nodiscard]] const abstract::ElementAdapter *
  element_adapter() const override;

protected:
  FileType m_file_type;
  DocumentType m_document_type;

  std::shared_ptr<abstract::ReadableFilesystem> m_files;

  ElementIdentifier m_root_element;
  std::unique_ptr<abstract::ElementAdapter> m_element_adapter;
};

} // namespace odr::internal
