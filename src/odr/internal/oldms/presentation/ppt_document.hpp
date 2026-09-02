#pragma once

#include <odr/internal/common/document.hpp>
#include <odr/internal/oldms/presentation/ppt_element_registry.hpp>
#include <odr/internal/oldms/presentation/ppt_style.hpp>

#include <memory>

namespace odr::internal::oldms::presentation {

class Document final : public internal::Document {
public:
  explicit Document(std::shared_ptr<abstract::ReadableFilesystem> files);

  ElementRegistry &element_registry();

  [[nodiscard]] const ElementRegistry &element_registry() const;

  [[nodiscard]] const StyleRegistry &style_registry() const;

private:
  ElementRegistry m_element_registry;
  StyleRegistry m_style_registry;
};

} // namespace odr::internal::oldms::presentation
