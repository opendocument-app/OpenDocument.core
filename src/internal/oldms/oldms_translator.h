#ifndef ODR_INTERNAL_OLDMS_TRANSLATOR_H
#define ODR_INTERNAL_OLDMS_TRANSLATOR_H

#include <internal/abstract/document_translator.h>
#include <memory>
#include <odr/file_meta.h>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::oldms {

class LegacyMicrosoftTranslator final : public abstract::DocumentTranslator {
public:
  explicit LegacyMicrosoftTranslator(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);
  LegacyMicrosoftTranslator(const LegacyMicrosoftTranslator &) = delete;
  LegacyMicrosoftTranslator(LegacyMicrosoftTranslator &&) noexcept;
  ~LegacyMicrosoftTranslator() final;
  LegacyMicrosoftTranslator &
  operator=(const LegacyMicrosoftTranslator &) = delete;
  LegacyMicrosoftTranslator &operator=(LegacyMicrosoftTranslator &&) noexcept;

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
  FileMeta m_meta;
};

} // namespace odr::internal::oldms

#endif // ODR_INTERNAL_OLDMS_TRANSLATOR_H
