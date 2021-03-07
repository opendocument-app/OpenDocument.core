#ifndef ODR_ABSTRACT_DOCUMENT_TRANSLATOR_H
#define ODR_ABSTRACT_DOCUMENT_TRANSLATOR_H

#include <odr/meta.h>

namespace odr {
struct Config;
}

namespace odr::internal::common {
class Path;
}

namespace odr::internal::abstract {

class DocumentTranslator {
public:
  virtual ~DocumentTranslator() = default;

  virtual const FileMeta &meta() const noexcept = 0;

  virtual bool decrypted() const noexcept = 0;
  virtual bool translatable() const noexcept = 0;
  virtual bool editable() const noexcept = 0;
  virtual bool savable(bool encrypted) const noexcept = 0;

  virtual bool decrypt(const std::string &password) = 0;

  virtual bool translate(const common::Path &path, const Config &config) = 0;

  virtual bool edit(const std::string &diff) = 0;

  virtual bool save(const common::Path &path) const = 0;
  virtual bool save(const common::Path &path,
                    const std::string &password) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_ABSTRACT_DOCUMENT_TRANSLATOR_H
