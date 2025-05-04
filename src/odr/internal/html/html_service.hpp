#pragma once

#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/html_service.hpp>

namespace odr::internal::html {

class HtmlService : public abstract::HtmlService {
public:
  HtmlService(HtmlConfig config, HtmlResourceLocator resource_locator);

  [[nodiscard]] const HtmlConfig &config() const override;
  [[nodiscard]] const HtmlResourceLocator &resource_locator() const override;

private:
  HtmlConfig m_config;
  HtmlResourceLocator m_resource_locator;
};

class HtmlView : public abstract::HtmlView {
public:
  HtmlView(const abstract::HtmlService &service, std::string name,
           std::string path);

  [[nodiscard]] const std::string &name() const override;
  [[nodiscard]] const std::string &path() const override;
  [[nodiscard]] const HtmlConfig &config() const override;
  [[nodiscard]] const abstract::HtmlService &service() const;

  HtmlResources write_html(html::HtmlWriter &out) const override;

private:
  const abstract::HtmlService *m_service;
  std::string m_name;
  std::string m_path;
};

class HtmlResource : public abstract::HtmlResource {
public:
  static odr::HtmlResource create(HtmlResourceType type, std::string mime_type,
                                  std::string name, std::string path,
                                  odr::File file, bool is_shipped,
                                  bool is_relocatable, bool is_external);

  HtmlResource(HtmlResourceType type, std::string mime_type, std::string name,
               std::string path, odr::File file, bool is_shipped,
               bool is_relocatable, bool is_external);

  [[nodiscard]] HtmlResourceType type() const override;
  [[nodiscard]] const std::string &mime_type() const override;
  [[nodiscard]] const std::string &name() const override;
  [[nodiscard]] const std::string &path() const override;
  [[nodiscard]] const odr::File &file() const override;
  [[nodiscard]] bool is_shipped() const override;
  [[nodiscard]] bool is_relocatable() const override;
  [[nodiscard]] bool is_external() const override;

  void write_resource(std::ostream &os) const override;

private:
  HtmlResourceType m_type;
  std::string m_mime_type;
  std::string m_name;
  std::string m_path;
  odr::File m_file;
  bool m_is_shipped{};
  bool m_is_relocatable{};
  bool m_is_external{};
};

} // namespace odr::internal::html
