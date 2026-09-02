#pragma once

#include <odr/internal/common/document.hpp>
#include <odr/internal/rtf/rtf_element_registry.hpp>

namespace odr::internal::abstract {
class File;
}

namespace odr::internal::rtf {

/// An rtf as a text document. Not backed by a filesystem: the whole file is
/// one byte stream, parsed into the element registry at construction.
class Document final : public internal::Document {
public:
  explicit Document(const abstract::File &file);

  [[nodiscard]] const ElementRegistry &element_registry() const;

private:
  ElementRegistry m_element_registry;
};

} // namespace odr::internal::rtf
