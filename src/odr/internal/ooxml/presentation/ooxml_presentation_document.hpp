#ifndef ODR_INTERNAL_OOXML_PRESENTATION_H
#define ODR_INTERNAL_OOXML_PRESENTATION_H

#include <odr/internal/abstract/document.hpp>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::ooxml::presentation {
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
  std::unordered_map<std::string, pugi::xml_document> m_slides_xml;

  friend class Element;
};

} // namespace odr::internal::ooxml::presentation

#endif // ODR_INTERNAL_OOXML_PRESENTATION_H
