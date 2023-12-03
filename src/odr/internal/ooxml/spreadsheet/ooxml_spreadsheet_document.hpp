#ifndef ODR_INTERNAL_OOXML_SPREADSHEET_DOCUMENT_H
#define ODR_INTERNAL_OOXML_SPREADSHEET_DOCUMENT_H

#include <odr/file.hpp>

#include <odr/internal/common/document.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_style.hpp>

#include <memory>
#include <string>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {

class Document final : public common::TemplateDocument<Element> {
public:
  explicit Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] bool is_editable() const noexcept final;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept final;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const char *password) const final;

private:
  struct Sheet final {
    common::Path sheet_path;
    pugi::xml_document sheet_xml;
    std::optional<common::Path> drawing_path;
    pugi::xml_document drawing_xml;
  };

  pugi::xml_document m_workbook_xml;
  pugi::xml_document m_styles_xml;
  std::unordered_map<std::string, Sheet> m_sheets;
  std::unordered_map<std::string, pugi::xml_document> m_drawings_xml;
  pugi::xml_document m_shared_strings_xml;

  StyleRegistry m_style_registry;
  std::vector<pugi::xml_node> m_shared_strings;

  friend class Element;
};

} // namespace odr::internal::ooxml::spreadsheet

#endif // ODR_INTERNAL_OOXML_SPREADSHEET_DOCUMENT_H
