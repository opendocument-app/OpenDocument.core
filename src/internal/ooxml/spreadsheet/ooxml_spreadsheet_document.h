#ifndef ODR_INTERNAL_OOXML_SPREADSHEET_DOCUMENT_H
#define ODR_INTERNAL_OOXML_SPREADSHEET_DOCUMENT_H

#include <internal/abstract/document.h>
#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {
class Element;

class Document final : public abstract::Document {
public:
  explicit Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] bool editable() const noexcept final;
  [[nodiscard]] bool savable(bool encrypted) const noexcept final;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const char *password) const final;

  [[nodiscard]] DocumentType document_type() const noexcept final;
  [[nodiscard]] std::shared_ptr<abstract::ReadableFilesystem>
  files() const noexcept final;

  [[nodiscard]] std::unique_ptr<abstract::DocumentCursor>
  root_element() const final;

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  pugi::xml_document m_workbook_xml;
  pugi::xml_document m_styles_xml;
  std::unordered_map<std::string, pugi::xml_document> m_sheets_xml;

  friend class Element;
};

} // namespace odr::internal::ooxml::spreadsheet

#endif // ODR_INTERNAL_OOXML_SPREADSHEET_DOCUMENT_H
