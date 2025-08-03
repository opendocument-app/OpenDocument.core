#include <odr/global_params.hpp>

#include <odr/internal/project_info.hpp>

#ifdef ODR_WITH_PDF2HTMLEX
#include <poppler/GlobalParams.h>
#endif

#ifdef ODR_WITH_CUSTOM_TMPFILE
#include <tmpfile.h>
#endif

namespace odr {

GlobalParams &GlobalParams::instance() {
  struct HolderAndInitializer {
    GlobalParams params;
    HolderAndInitializer() {
#ifdef ODR_WITH_PDF2HTMLEX
      globalParams = std::make_unique<::GlobalParams>(
          params.m_poppler_data_path.empty()
              ? nullptr
              : params.m_poppler_data_path.c_str());
#endif
    }
  };
  static HolderAndInitializer instance;

  return instance.params;
}

const std::string &GlobalParams::odr_core_data_path() {
  return instance().m_odr_core_data_path;
}

const std::string &GlobalParams::fontconfig_data_path() {
  return instance().m_fontconfig_data_path;
}

const std::string &GlobalParams::poppler_data_path() {
  return instance().m_poppler_data_path;
}

const std::string &GlobalParams::pdf2htmlex_data_path() {
  return instance().m_pdf2htmlex_data_path;
}

const std::string &GlobalParams::custom_tmpfile_path() {
  return instance().m_custom_tmpfile_path;
}

void GlobalParams::set_odr_core_data_path(const std::string &path) {
  instance().m_odr_core_data_path = path;
}

void GlobalParams::set_fontconfig_data_path(const std::string &path) {
  instance().m_fontconfig_data_path = path;
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

void GlobalParams::set_custom_tmpfile_path(const std::string &path) {
#ifdef ODR_WITH_CUSTOM_TMPFILE
  set_tmpfile_directory(path.c_str());
#endif

  instance().m_custom_tmpfile_path = path;
}

GlobalParams::GlobalParams()
    : m_odr_core_data_path{internal::project_info::odr_data_path()},
      m_fontconfig_data_path{internal::project_info::fontconfig_data_path()},
      m_poppler_data_path{internal::project_info::poppler_data_path()},
      m_pdf2htmlex_data_path{internal::project_info::pdf2htmlex_data_path()} {}

} // namespace odr
