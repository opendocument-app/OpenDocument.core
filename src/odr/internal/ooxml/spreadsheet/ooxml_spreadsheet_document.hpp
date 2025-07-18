#pragma once

#include <odr/file.hpp>

#include <odr/internal/common/document.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_element.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_style.hpp>

#include <memory>
#include <string>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {

class Document final : public TemplateDocument<Element> {
public:
  explicit Document(std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] bool is_editable() const noexcept final;
  [[nodiscard]] bool is_savable(bool encrypted) const noexcept final;

  void save(const Path &path) const final;
  void save(const Path &path, const char *password) const final;

  std::pair<const pugi::xml_document &, const Relations &>
  get_xml(const Path &) const;
  pugi::xml_node get_shared_string(std::size_t index) const;

private:
  std::unordered_map<Path, std::pair<pugi::xml_document, Relations>> m_xml;

  StyleRegistry m_style_registry;
  std::vector<pugi::xml_node> m_shared_strings;

  std::pair<pugi::xml_document &, Relations &> parse_xml_(const AbsPath &path);

  friend class Element;
};

} // namespace odr::internal::ooxml::spreadsheet
