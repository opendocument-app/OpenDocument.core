#ifndef ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H
#define ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H

#include <odr/file.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/ooxml/text/ooxml_text_style.hpp>

#include <pugixml.hpp>

#include <memory>
#include <string>
#include <unordered_map>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::ooxml::text {
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
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  pugi::xml_document m_document_xml;
  pugi::xml_document m_styles_xml;

  StyleRegistry m_style_registry;

  std::unordered_map<std::string, std::string> m_document_relations;

  friend class Element;
  friend class DocumentCursor;
};

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_DOCUMENT_H
