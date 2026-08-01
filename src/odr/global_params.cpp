#include <odr/global_params.hpp>

#include <odr/internal/project_info.hpp>

namespace odr {

GlobalParams &GlobalParams::instance() {
  static GlobalParams instance;

  return instance;
}

const std::string &GlobalParams::odr_core_data_path() {
  return instance().m_odr_core_data_path;
}

const std::string &GlobalParams::libmagic_database_path() {
  return instance().m_libmagic_database_path;
}

void GlobalParams::set_odr_core_data_path(const std::string &path) {
  instance().m_odr_core_data_path = path;
}

void GlobalParams::set_libmagic_database_path(const std::string &path) {
  instance().m_libmagic_database_path = path;
}

// `m_libmagic_database_path` starts empty: nothing reads it, and there is no
// longer a database path to default it to.
GlobalParams::GlobalParams()
    : m_odr_core_data_path{internal::project_info::odr_data_path()} {}

} // namespace odr
