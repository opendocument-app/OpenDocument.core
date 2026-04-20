#pragma once

#include <odr/internal/common/document.hpp>
#include <odr/internal/oldms/text/doc_element_registry.hpp>

#include <memory>

namespace odr::internal::oldms::text {

class Document final : public internal::Document {
public:
  explicit Document(std::shared_ptr<abstract::ReadableFilesystem> files);

  ElementRegistry &element_registry();

  [[nodiscard]] const ElementRegistry &element_registry() const;

  [[nodiscard]] bool is_editable() const noexcept override;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept override;

  void save(const Path &path) const override;
  void save(const Path &path, const char *password) const override;

private:
  ElementRegistry m_element_registry;
};

} // namespace odr::internal::oldms::text
