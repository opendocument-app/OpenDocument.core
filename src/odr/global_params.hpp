#pragma once

#include <string>

namespace odr {

class GlobalParams final {
public:
  static const std::string &odr_core_data_path();
  /// @deprecated Read only by a build with the deprecated `ODR_WITH_LIBMAGIC`.
  /// Detection is our own otherwise, and looks at nothing outside the file.
  static const std::string &libmagic_database_path();

  static void set_odr_core_data_path(const std::string &path);
  /// @deprecated See @ref libmagic_database_path.
  static void set_libmagic_database_path(const std::string &path);

private:
  static GlobalParams &instance();

  GlobalParams();

  std::string m_odr_core_data_path;
  std::string m_libmagic_database_path;
};

} // namespace odr
