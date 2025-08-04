#pragma once

#include <string>

namespace odr {

class GlobalParams {
public:
  static const std::string &odr_core_data_path();
  static const std::string &fontconfig_data_path();
  static const std::string &poppler_data_path();
  static const std::string &pdf2htmlex_data_path();
  static const std::string &custom_tmpfile_path();

  static void set_odr_core_data_path(const std::string &path);
  static void set_fontconfig_data_path(const std::string &path);
  static void set_poppler_data_path(const std::string &path);
  static void set_pdf2htmlex_data_path(const std::string &path);
  static void set_custom_tmpfile_path(const std::string &path);

private:
  static GlobalParams &instance();

  GlobalParams();

  std::string m_odr_core_data_path;
  std::string m_fontconfig_data_path;
  std::string m_poppler_data_path;
  std::string m_pdf2htmlex_data_path;
  std::string m_custom_tmpfile_path;
};

} // namespace odr
