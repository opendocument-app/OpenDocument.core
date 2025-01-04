#pragma once

#include "odr/file.hpp"

#include "odr/internal/abstract/html_service.hpp"

namespace odr::internal::common {

class StaticHtmlResource : public abstract::HtmlResource {
public:
  static odr::HtmlResource create(HtmlResourceType type,
                                  const std::string &name,
                                  const std::string &path,
                                  const odr::File &file, bool is_shipped,
                                  bool is_relocatable);

  StaticHtmlResource(HtmlResourceType type, const std::string &name,
                     const std::string &path, const odr::File &file,
                     bool is_shipped, bool is_relocatable);

  [[nodiscard]] HtmlResourceType type() const override;
  [[nodiscard]] const std::string &name() const override;
  [[nodiscard]] const std::string &path() const override;
  [[nodiscard]] const odr::File &file() const override;
  [[nodiscard]] bool is_shipped() const override;
  [[nodiscard]] bool is_relocatable() const override;

  void write_resource(std::ostream &os) const override;

private:
  HtmlResourceType m_type;
  std::string m_name;
  std::string m_path;
  odr::File m_file;
  bool m_is_shipped;
  bool m_is_relocatable;
};

} // namespace odr::internal::common
