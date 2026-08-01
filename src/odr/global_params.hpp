#pragma once

#include <string>

namespace odr {

class GlobalParams final {
public:
  /// @deprecated Inert: the css and js are part of the library and nothing is
  /// read from disk. It still stores and returns whatever is set, so a caller
  /// that points it somewhere keeps working.
  static const std::string &odr_core_data_path();
  /// @deprecated Inert: libmagic is gone and nothing reads this. It still
  /// stores and returns whatever is set, so a caller that sets it keeps
  /// working — detection is our own now, and looks at nothing outside the file.
  static const std::string &libmagic_database_path();

  /// @deprecated See @ref odr_core_data_path.
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
