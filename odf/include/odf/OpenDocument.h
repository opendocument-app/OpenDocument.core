#ifndef ODR_ODF_OPENDOCUMENT_H
#define ODR_ODF_OPENDOCUMENT_H

#include <common/Document.h>
#include <memory>

namespace odr {
namespace access {
class ReadStorage;
}

namespace odf {

class OpenDocumentFile final {
public:
  explicit OpenDocumentFile(const char *path);
  explicit OpenDocumentFile(const std::string &path);
  explicit OpenDocumentFile(const access::Path &path);
  explicit OpenDocumentFile(std::unique_ptr<access::ReadStorage> &&storage);
  explicit OpenDocumentFile(std::unique_ptr<access::ReadStorage> &storage);
  OpenDocumentFile(const OpenDocumentFile &) = delete;
  OpenDocumentFile(OpenDocumentFile &&) noexcept;
  ~OpenDocumentFile();
  OpenDocumentFile &operator=(const OpenDocumentFile &) = delete;
  OpenDocumentFile &operator=(OpenDocumentFile &&) noexcept;

  const FileMeta &meta() const noexcept;
  const access::ReadStorage &storage() const noexcept;
};

class OpenDocument final : public common::Document {
public:
  explicit OpenDocument(const char *path);
  explicit OpenDocument(const std::string &path);
  explicit OpenDocument(const access::Path &path);
  explicit OpenDocument(std::unique_ptr<access::ReadStorage> &&storage);
  explicit OpenDocument(std::unique_ptr<access::ReadStorage> &storage);
  OpenDocument(const OpenDocument &) = delete;
  OpenDocument(OpenDocument &&) noexcept;
  ~OpenDocument() final;
  OpenDocument &operator=(const OpenDocument &) = delete;
  OpenDocument &operator=(OpenDocument &&) noexcept;

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

#endif // ODR_ODF_OPENDOCUMENT_H
