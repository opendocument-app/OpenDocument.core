#include <odr/global_params.hpp>

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

// Both paths start empty: nothing reads either of them, and there is no longer
// anything on disk to default them to.
GlobalParams::GlobalParams() = default;

} // namespace odr
