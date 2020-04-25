#ifndef ODR_OOXML_OFFICE_OPEN_XML_H
#define ODR_OOXML_OFFICE_OPEN_XML_H

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
namespace ooxml {

class OfficeOpenXml final {
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
  ~OfficeOpenXml();

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

} // namespace ooxml
} // namespace odr

#endif // ODR_OOXML_OFFICE_OPEN_XML_H
