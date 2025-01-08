#include <odr/global_params.hpp>

#include <odr/internal/project_info.hpp>

#ifdef ODR_WITH_PDF2HTMLEX
#include <poppler/GlobalParams.h>
#endif

namespace odr {

GlobalParams &GlobalParams::instance() {
  static GlobalParams instance;
  return instance;
}

const std::string &GlobalParams::odr_core_data_path() {
  return instance().m_odr_core_data_path;
}

const std::string &GlobalParams::fontforge_data_path() {
  return instance().m_fontforge_data_path;
}

const std::string &GlobalParams::poppler_data_path() {
  return instance().m_poppler_data_path;
}

const std::string &GlobalParams::pdf2htmlex_data_path() {
  return instance().m_pdf2htmlex_data_path;
}

void GlobalParams::set_odr_core_data_path(const std::string &path) {
  instance().m_odr_core_data_path = path;
}

void GlobalParams::set_fontforge_data_path(const std::string &path) {
  instance().m_fontforge_data_path = path;
}

void GlobalParams::set_poppler_data_path(const std::string &path) {
  instance().m_poppler_data_path = path;

#ifdef ODR_WITH_PDF2HTMLEX
  globalParams =
      std::make_unique<::GlobalParams>(path.empty() ? nullptr : path.c_str());
#endif
}

void GlobalParams::set_pdf2htmlex_data_path(const std::string &path) {
  instance().m_pdf2htmlex_data_path = path;
}

GlobalParams::GlobalParams() {
  set_odr_core_data_path(""); // TODO
  set_fontforge_data_path(internal::project_info::fontconfig_data_path());
  set_poppler_data_path(internal::project_info::poppler_data_path());
  set_pdf2htmlex_data_path(internal::project_info::pdf2htmlex_data_path());
}

} // namespace odr
