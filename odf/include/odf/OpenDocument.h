#ifndef ODR_ODF_OPEN_DOCUMENT_H
#define ODR_ODF_OPEN_DOCUMENT_H

#include <memory>
#include <odr/Config.h>
#include <odr/Meta.h>

namespace odr {
namespace access {
class Path;
class ReadStorage;
} // namespace access
} // namespace odr

namespace odr {
namespace odf {

class OpenDocument final {
public:
  explicit OpenDocument(const char *path);
  explicit OpenDocument(const std::string &path);
  explicit OpenDocument(const access::Path &path);
  explicit OpenDocument(std::unique_ptr<access::ReadStorage> &&storage);
  explicit OpenDocument(std::unique_ptr<access::ReadStorage> &storage);
  OpenDocument(const OpenDocument &) = delete;
  OpenDocument(OpenDocument &&) noexcept;
  OpenDocument &operator=(const OpenDocument &) = delete;
  OpenDocument &operator=(OpenDocument &&) noexcept;
  ~OpenDocument();

  FileType type() const noexcept;
  bool encrypted() const noexcept;
  const FileMeta &meta() const noexcept;
  const access::ReadStorage &storage() const noexcept;

  bool decrypted() const noexcept;
  bool canHtml() const noexcept;
  bool canEdit() const noexcept;
  bool canSave(bool encrypted = false) const noexcept;

  bool decrypt(const std::string &password);

  bool html(const access::Path &path, const Config &config);
  bool edit(const std::string &diff);

  bool save(const access::Path &path) const;
  bool save(const access::Path &path, const std::string &password) const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace odf
} // namespace odr

#endif // ODR_ODF_OPEN_DOCUMENT_H
