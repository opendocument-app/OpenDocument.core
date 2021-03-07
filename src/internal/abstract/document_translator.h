#ifndef ODR_ABSTRACT_DOCUMENT_TRANSLATOR_H
#define ODR_ABSTRACT_DOCUMENT_TRANSLATOR_H

namespace odr {
struct HtmlConfig;
struct FileMeta;
} // namespace odr

namespace odr::internal::common {
class Path;
}

namespace odr::internal::abstract {

class DocumentTranslator {
public:
  virtual ~DocumentTranslator() = default;

  [[nodiscard]] virtual const FileMeta &meta() const noexcept = 0;

  [[nodiscard]] virtual bool decrypted() const noexcept = 0;
  [[nodiscard]] virtual bool translatable() const noexcept = 0;
  [[nodiscard]] virtual bool editable() const noexcept = 0;
  [[nodiscard]] virtual bool savable(bool encrypted) const noexcept = 0;

  virtual bool decrypt(const std::string &password) = 0;

  virtual bool translate(const common::Path &path,
                         const HtmlConfig &config) = 0;

  virtual bool edit(const std::string &diff) = 0;

  virtual bool save(const common::Path &path) const = 0;
  virtual bool save(const common::Path &path,
                    const std::string &password) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_ABSTRACT_DOCUMENT_TRANSLATOR_H
