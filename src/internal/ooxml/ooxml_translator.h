#ifndef ODR_INTERNAL_OOXML_TRANSLATOR_H
#define ODR_INTERNAL_OOXML_TRANSLATOR_H

#include <internal/abstract/document_translator.h>
#include <internal/ooxml/ooxml_translator_context.h>
#include <memory>
#include <odr/file_meta.h>
#include <pugixml.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::ooxml {

class OfficeOpenXmlTranslator final : public abstract::DocumentTranslator {
public:
  explicit OfficeOpenXmlTranslator(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);
  OfficeOpenXmlTranslator(const OfficeOpenXmlTranslator &) = delete;
  OfficeOpenXmlTranslator(OfficeOpenXmlTranslator &&) noexcept = default;
  ~OfficeOpenXmlTranslator() final;
  OfficeOpenXmlTranslator &operator=(const OfficeOpenXmlTranslator &) = delete;
  OfficeOpenXmlTranslator &
  operator=(OfficeOpenXmlTranslator &&) noexcept = default;

  [[nodiscard]] const FileMeta &meta() const noexcept final;

  [[nodiscard]] bool decrypted() const noexcept final;
  [[nodiscard]] bool translatable() const noexcept final;
  [[nodiscard]] bool editable() const noexcept final;
  [[nodiscard]] bool savable(bool encrypted) const noexcept final;

  bool decrypt(const std::string &password) final;

  void translate(const common::Path &path, const HtmlConfig &config) final;

  void edit(const std::string &diff) final;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const std::string &password) const final;

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;

  FileMeta m_meta;

  bool m_decrypted{false};

  Context m_context;
  pugi::xml_document m_style;
  pugi::xml_document m_content;
};

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_TRANSLATOR_H
