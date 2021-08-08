#ifndef ODR_INTERNAL_ODF_TRANSLATOR_H
#define ODR_INTERNAL_ODF_TRANSLATOR_H

#include <internal/abstract/document_translator.h>
#include <internal/odf/odf_manifest.h>
#include <internal/odf/odf_translator_context.h>
#include <memory>
#include <odr/file_meta.h>
#include <pugixml.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::common {
class Path;
}

namespace odr::internal::odf {

class OpenDocumentTranslator final : public abstract::DocumentTranslator {
public:
  explicit OpenDocumentTranslator(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);
  OpenDocumentTranslator(const OpenDocumentTranslator &) = delete;
  OpenDocumentTranslator(OpenDocumentTranslator &&) noexcept = default;
  ~OpenDocumentTranslator() final;
  OpenDocumentTranslator &operator=(const OpenDocumentTranslator &) = delete;
  OpenDocumentTranslator &
  operator=(OpenDocumentTranslator &&) noexcept = default;

  [[nodiscard]] const FileMeta &meta() const noexcept final;
  [[nodiscard]] const abstract::ReadableFilesystem &filesystem() const noexcept;

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
  Manifest m_manifest;

  bool m_decrypted{false};

  Context m_context;
  pugi::xml_document m_style;
  pugi::xml_document m_content;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_TRANSLATOR_H
