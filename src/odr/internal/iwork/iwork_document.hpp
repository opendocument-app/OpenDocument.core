#pragma once

#include <odr/file.hpp>

#include <odr/internal/common/document.hpp>
#include <odr/internal/iwork/iwork_element_registry.hpp>

#include <memory>

namespace odr::internal::iwork {

/// Read-only iWork document: Pages text, Keynote slides or Numbers sheets.
class Document final : public internal::Document {
public:
  Document(FileType file_type,
           std::shared_ptr<abstract::ReadableFilesystem> files);

  [[nodiscard]] const ElementRegistry &element_registry() const;

private:
  ElementRegistry m_element_registry;
};

} // namespace odr::internal::iwork
