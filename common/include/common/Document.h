#ifndef ODR_COMMON_DOCUMENT_H
#define ODR_COMMON_DOCUMENT_H

#include <odr/Meta.h>

namespace odr {
struct Config;

namespace access {
class Path;
}

namespace common {

class Document {
public:
  virtual ~Document() = default;

  virtual const FileMeta &meta() const noexcept = 0;

  virtual bool decrypted() const noexcept = 0;
  virtual bool translatable() const noexcept = 0;
  virtual bool editable() const noexcept = 0;
  virtual bool savable(bool encrypted) const noexcept = 0;

  virtual bool decrypt(const std::string &password) = 0;

  virtual void translate(const access::Path &path, const Config &config) = 0;

  virtual void edit(const std::string &diff) = 0;

  virtual void save(const access::Path &path) const = 0;
  virtual void save(const access::Path &path,
                    const std::string &password) const = 0;
};

} // namespace common
} // namespace odr

#endif // ODR_COMMON_DOCUMENT_H
