#ifndef ODR_ODF_OPEN_DOCUMENT_H
#define ODR_ODF_OPEN_DOCUMENT_H

#include <common/Document.h>
#include <memory>

namespace odr {
namespace access {
class ReadStorage;
}

namespace odf {

class OpenDocument final : public common::Document {
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
  ~OpenDocument() final;

  const FileMeta &meta() const noexcept;
  const access::ReadStorage &storage() const noexcept;

  bool decrypted() const noexcept final;
  bool translatable() const noexcept final;
  bool editable() const noexcept final;
  bool savable(bool encrypted) const noexcept final;

  bool decrypt(const std::string &password) final;

  void translate(const access::Path &path, const Config &config) final;

  void edit(const std::string &diff) final;

  void save(const access::Path &path) const final;
  void save(const access::Path &path, const std::string &password) const final;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace odf
} // namespace odr

#endif // ODR_ODF_OPEN_DOCUMENT_H
