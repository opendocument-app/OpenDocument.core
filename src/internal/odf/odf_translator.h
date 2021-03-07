#ifndef ODR_INTERNAL_ODF_TRANSLATOR_H
#define ODR_INTERNAL_ODF_TRANSLATOR_H

#include <internal/abstract/document_translator.h>
#include <memory>
#include <pugixml.hpp>

namespace odr::internal::abstract {
class ReadFilesystem;
}

namespace odr::internal::common {
class Path;
}

namespace odr::internal::odf {

class OpenDocumentTranslator final : public abstract::DocumentTranslator {
public:
  explicit OpenDocumentTranslator(const char *path);
  explicit OpenDocumentTranslator(const std::string &path);
  explicit OpenDocumentTranslator(const common::Path &path);
  explicit OpenDocumentTranslator(
      std::unique_ptr<abstract::ReadFilesystem> &&storage);
  explicit OpenDocumentTranslator(
      std::unique_ptr<abstract::ReadFilesystem> &storage);
  OpenDocumentTranslator(const OpenDocumentTranslator &) = delete;
  OpenDocumentTranslator(OpenDocumentTranslator &&) noexcept;
  ~OpenDocumentTranslator() final;
  OpenDocumentTranslator &operator=(const OpenDocumentTranslator &) = delete;
  OpenDocumentTranslator &operator=(OpenDocumentTranslator &&) noexcept;

  [[nodiscard]] const FileMeta &meta() const noexcept final;
  [[nodiscard]] const abstract::ReadFilesystem &filesystem() const noexcept;

  [[nodiscard]] bool decrypted() const noexcept final;
  [[nodiscard]] bool translatable() const noexcept final;
  [[nodiscard]] bool editable() const noexcept final;
  [[nodiscard]] bool savable(bool encrypted) const noexcept final;

  bool decrypt(const std::string &password) final;

  bool translate(const common::Path &path, const Config &config) final;

  bool edit(const std::string &diff) final;

  bool save(const common::Path &path) const final;
  bool save(const common::Path &path, const std::string &password) const final;

private:
  std::unique_ptr<abstract::ReadFilesystem> m_filesystem;

  FileMeta m_meta;
  Meta::Manifest m_manifest;

  bool m_decrypted{false};

  Context m_context;
  pugi::xml_document m_style;
  pugi::xml_document m_content;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_TRANSLATOR_H
