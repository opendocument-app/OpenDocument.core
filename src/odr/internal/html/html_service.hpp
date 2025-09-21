#pragma once

#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/html_service.hpp>

namespace odr::internal::html {

class HtmlService : public abstract::HtmlService {
public:
  HtmlService(HtmlConfig config, std::shared_ptr<Logger> logger);

  [[nodiscard]] const HtmlConfig &config() const override;

private:
  HtmlConfig m_config;

protected:
  std::shared_ptr<Logger> m_logger;
};

class HtmlView : public abstract::HtmlView {
public:
  HtmlView(const abstract::HtmlService &service, std::string name,
           std::string path);

  [[nodiscard]] const std::string &name() const override;
  [[nodiscard]] const std::string &path() const override;
  [[nodiscard]] const HtmlConfig &config() const override;
  [[nodiscard]] const abstract::HtmlService &service() const;

  HtmlResources write_html(HtmlWriter &out) const override;

private:
  const abstract::HtmlService *m_service;
  std::string m_name;
  std::string m_path;
};

class HtmlResource : public abstract::HtmlResource {
public:
  static odr::HtmlResource create(HtmlResourceType type, std::string mime_type,
                                  std::string name, std::string path,
                                  std::optional<File> file, bool is_shipped,
                                  bool is_external, bool is_accessible);

  HtmlResource(HtmlResourceType type, std::string mime_type, std::string name,
               std::string path, std::optional<File> file, bool is_shipped,
               bool is_external, bool is_accessible);

  [[nodiscard]] HtmlResourceType type() const override;
  [[nodiscard]] const std::string &mime_type() const override;
  [[nodiscard]] const std::string &name() const override;
  [[nodiscard]] const std::string &path() const override;
  [[nodiscard]] const std::optional<File> &file() const override;
  [[nodiscard]] bool is_shipped() const override;
  [[nodiscard]] bool is_external() const override;
  [[nodiscard]] bool is_accessible() const override;

  void write_resource(std::ostream &os) const override;

private:
  HtmlResourceType m_type;
  std::string m_mime_type;
  std::string m_name;
  std::string m_path;
  std::optional<File> m_file;
  bool m_is_shipped{};
  bool m_is_external{};
  bool m_is_accessible{};
};

} // namespace odr::internal::html
