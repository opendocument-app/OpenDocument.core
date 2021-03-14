#ifndef ODR_INTERNAL_OLDMS_TRANSLATOR_H
#define ODR_INTERNAL_OLDMS_TRANSLATOR_H

#include <internal/abstract/document_translator.h>
#include <memory>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::oldms {

class LegacyMicrosoft final : public abstract::DocumentTranslator {
public:
  explicit LegacyMicrosoft(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);
  LegacyMicrosoft(const LegacyMicrosoft &) = delete;
  LegacyMicrosoft(LegacyMicrosoft &&) noexcept;
  ~LegacyMicrosoft() final;
  LegacyMicrosoft &operator=(const LegacyMicrosoft &) = delete;
  LegacyMicrosoft &operator=(LegacyMicrosoft &&) noexcept;

  [[nodiscard]] const FileMeta &meta() const noexcept final;

  [[nodiscard]] bool decrypted() const noexcept final;
  [[nodiscard]] bool translatable() const noexcept final;
  [[nodiscard]] bool editable() const noexcept final;
  [[nodiscard]] bool savable(bool encrypted) const noexcept final;

  bool decrypt(const std::string &password) final;

  bool translate(const common::Path &path, const HtmlConfig &config) final;

  bool edit(const std::string &diff) final;

  bool save(const common::Path &path) const final;
  bool save(const common::Path &path, const std::string &password) const final;

private:
  FileMeta m_meta;
};

} // namespace odr::internal::oldms

#endif // ODR_INTERNAL_OLDMS_TRANSLATOR_H
