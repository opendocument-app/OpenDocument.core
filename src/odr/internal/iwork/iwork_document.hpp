#pragma once

#include <odr/file.hpp>

#include <odr/internal/common/document.hpp>
#include <odr/internal/iwork/iwork_element_registry.hpp>

#include <memory>

namespace odr::internal::iwork {

/// An iWork package, read as the kind of document the app that wrote it
/// makes: a `.pages` as text, a `.key` as a presentation, a `.numbers` as a
/// spreadsheet.
class Document final : public internal::Document {
public:
  Document(FileType file_type,
           std::shared_ptr<abstract::ReadableFilesystem> files);

  [[nodiscard]] const ElementRegistry &element_registry() const;

private:
  ElementRegistry m_element_registry;
};

} // namespace odr::internal::iwork
