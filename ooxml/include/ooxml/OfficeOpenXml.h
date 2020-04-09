#ifndef ODR_OOXML_OFFICE_OPEN_XML_H
#define ODR_OOXML_OFFICE_OPEN_XML_H

#include <memory>
#include <odr/Config.h>
#include <odr/Meta.h>

namespace odr {
namespace access {
class Path;
class Storage;
} // namespace access
} // namespace odr

namespace odr {
namespace ooxml {

class OfficeOpenXml final {
public:
  explicit OfficeOpenXml(const char *path);
  explicit OfficeOpenXml(const std::string &path);
  explicit OfficeOpenXml(const access::Path &path);
  explicit OfficeOpenXml(std::unique_ptr<access::Storage> &&storage);
  explicit OfficeOpenXml(std::unique_ptr<access::Storage> &storage);
  OfficeOpenXml(const OfficeOpenXml &) = delete;
  OfficeOpenXml(OfficeOpenXml &&) noexcept;
  OfficeOpenXml &operator=(const OfficeOpenXml &) = delete;
  OfficeOpenXml &operator=(OfficeOpenXml &&) noexcept;
  ~OfficeOpenXml();

  bool canHtml() const noexcept;
  bool canEdit() const noexcept;
  bool canSave(bool encrypted = false) const noexcept;
  const FileMeta &getMeta() const noexcept;
  const access::Storage &getStorage() const noexcept;

  bool html(const access::Path &path, const Config &config);
  bool edit(const std::string &diff);

  bool save(const access::Path &path) const;
  bool save(const access::Path &path, const std::string &password) const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace ooxml
} // namespace odr

#endif // ODR_OOXML_OFFICE_OPEN_XML_H
