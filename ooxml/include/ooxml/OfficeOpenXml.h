#ifndef ODR_OOXML_OFFICE_OPEN_XML_H
#define ODR_OOXML_OFFICE_OPEN_XML_H

#include <common/Document.h>
#include <memory>

namespace odr {
struct FileMeta;
struct Config;

namespace access {
class Path;
class ReadStorage;
} // namespace access

namespace ooxml {

class OfficeOpenXml final : public common::Document {
public:
  explicit OfficeOpenXml(const char *path);
  explicit OfficeOpenXml(const std::string &path);
  explicit OfficeOpenXml(const access::Path &path);
  explicit OfficeOpenXml(std::unique_ptr<access::ReadStorage> &&storage);
  explicit OfficeOpenXml(std::unique_ptr<access::ReadStorage> &storage);
  OfficeOpenXml(const OfficeOpenXml &) = delete;
  OfficeOpenXml(OfficeOpenXml &&) noexcept;
  OfficeOpenXml &operator=(const OfficeOpenXml &) = delete;
  OfficeOpenXml &operator=(OfficeOpenXml &&) noexcept;
  ~OfficeOpenXml() final;

  const FileMeta &meta() const noexcept final;
  const access::ReadStorage &storage() const noexcept;

  bool decrypted() const noexcept final;
  bool canTranslate() const noexcept final;
  bool canEdit() const noexcept final;
  bool canSave(bool encrypted) const noexcept final;

  bool decrypt(const std::string &password) final;

  void translate(const access::Path &path, const Config &config) final;

  void edit(const std::string &diff) final;

  void save(const access::Path &path) const final;
  void save(const access::Path &path, const std::string &password) const final;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace ooxml
} // namespace odr

#endif // ODR_OOXML_OFFICE_OPEN_XML_H
