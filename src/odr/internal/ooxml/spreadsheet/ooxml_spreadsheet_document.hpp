#ifndef ODR_INTERNAL_OOXML_SPREADSHEET_DOCUMENT_H
#define ODR_INTERNAL_OOXML_SPREADSHEET_DOCUMENT_H

#include <memory>
#include <odr/file.hpp>
#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_style.hpp>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>

namespace odr::internal::abstract {
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::ooxml::spreadsheet {
class Element;

class Document final : public abstract::Document {
public:
  explicit Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] bool editable() const noexcept final;
  [[nodiscard]] bool savable(bool encrypted) const noexcept final;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const char *password) const final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] DocumentType document_type() const noexcept final;

  [[nodiscard]] std::shared_ptr<abstract::ReadableFilesystem>
  files() const noexcept final;

  [[nodiscard]] std::unique_ptr<abstract::DocumentCursor>
  root_element() const final;

private:
  struct Sheet final {
    common::Path sheet_path;
    pugi::xml_document sheet_xml;
    std::optional<common::Path> drawing_path;
    pugi::xml_document drawing_xml;
  };

  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

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
