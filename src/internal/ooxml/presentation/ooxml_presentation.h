#ifndef ODR_INTERNAL_OOXML_PRESENTATION_H
#define ODR_INTERNAL_OOXML_PRESENTATION_H

#include <internal/abstract/document.h>
#include <internal/common/element.h>
#include <pugixml.hpp>

namespace odr::internal::ooxml {

class OfficeOpenXmlPresentation final : public abstract::Document {
public:
  explicit OfficeOpenXmlPresentation(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] bool editable() const noexcept final;
  [[nodiscard]] bool savable(bool encrypted) const noexcept final;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const char *password) const final;

  [[nodiscard]] DocumentType document_type() const noexcept final;
  [[nodiscard]] std::shared_ptr<abstract::ReadableFilesystem>
  files() const noexcept final;

  [[nodiscard]] odr::Element root_element() const final;

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  odr::Element m_root;
};

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_PRESENTATION_H
