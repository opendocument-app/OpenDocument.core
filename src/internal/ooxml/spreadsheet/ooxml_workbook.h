#ifndef ODR_INTERNAL_OOXML_WORKBOOK_H
#define ODR_INTERNAL_OOXML_WORKBOOK_H

#include <internal/abstract/document.h>
#include <internal/common/element.h>
#include <pugixml.hpp>

namespace odr::internal::ooxml {

/*
 * TODO naming - spreadsheet?
 */
class OfficeOpenXmlWorkbook final : public abstract::Document {
public:
  explicit OfficeOpenXmlWorkbook(
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

  pugi::xml_document m_styles_xml;
  std::vector<pugi::xml_document> m_sheets_xml;

  odr::Element m_root;
};

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_WORKBOOK_H
