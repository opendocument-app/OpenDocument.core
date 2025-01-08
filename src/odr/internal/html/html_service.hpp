#pragma once

#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/html_service.hpp>

namespace odr::internal::html {

class HtmlService : public abstract::HtmlService {
public:
  HtmlService(HtmlConfig config, HtmlResourceLocator resource_locator,
              std::vector<std::shared_ptr<abstract::HtmlFragment>> fragments);

  [[nodiscard]] const HtmlConfig &config() const override;
  [[nodiscard]] const HtmlResourceLocator &resource_locator() const override;

  [[nodiscard]] const std::vector<std::shared_ptr<abstract::HtmlFragment>> &
  fragments() const override;

private:
  HtmlConfig m_config;
  HtmlResourceLocator m_resource_locator;
  std::vector<std::shared_ptr<abstract::HtmlFragment>> m_fragments;
};

class HtmlFragment : public abstract::HtmlFragment {
public:
  HtmlFragment(std::string name, HtmlConfig config,
               HtmlResourceLocator resource_locator);

  [[nodiscard]] std::string name() const override;

  [[nodiscard]] const HtmlConfig &config() const override;
  [[nodiscard]] const HtmlResourceLocator &resource_locator() const override;

private:
  std::string m_name;
  HtmlConfig m_config;
  HtmlResourceLocator m_resource_locator;
};

class HtmlResource : public abstract::HtmlResource {
public:
  static odr::HtmlResource create(HtmlResourceType type,
                                  const std::string &name,
                                  const std::string &path,
                                  const odr::File &file, bool is_shipped,
                                  bool is_relocatable);

  HtmlResource(HtmlResourceType type, const std::string &name,
               const std::string &path, const odr::File &file, bool is_shipped,
               bool is_relocatable);

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

} // namespace odr::internal::html
