#ifndef ODR_OOXML_OFFICE_OPEN_XML_H
#define ODR_OOXML_OFFICE_OPEN_XML_H

#include <common/Document.h>
#include <memory>

namespace odr {
namespace access {
class ReadStorage;
}

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
  ~OfficeOpenXml() final;
  OfficeOpenXml &operator=(const OfficeOpenXml &) = delete;
  OfficeOpenXml &operator=(OfficeOpenXml &&) noexcept;

  DocumentMeta documentMeta() const noexcept final;
  const access::ReadStorage &storage() const noexcept;

  bool editable() const noexcept final;
  bool savable(bool encrypted) const noexcept final;

  void save(const access::Path &path) const final;
  void save(const access::Path &path, const std::string &password) const final;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace ooxml
} // namespace odr

#endif // ODR_OOXML_OFFICE_OPEN_XML_H
