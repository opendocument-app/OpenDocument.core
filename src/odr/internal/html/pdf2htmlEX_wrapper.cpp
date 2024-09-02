#include <odr/internal/html/pdf2htmlEX_wrapper.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/file.hpp>

#include <pdf2htmlEX.h>

#include <odr/internal/project_info.hpp>

#include <fontconfig/fontconfig.h>

namespace odr::internal {

// Sample fontconfig code taken from
// https://gitlab.com/camconn/fontconfig-example/-/blob/master/fc-match.c?ref_type=heads
static int fc_test(const char *query) {
  FcConfig *conf;
  FcFontSet *fs;
  FcObjectSet *os;
  FcPattern *pat;
  FcResult result;

  conf = FcInitLoadConfigAndFonts();
  pat = FcNameParse((FcChar8 *)query);

  if (!pat)
    return 2;

  FcConfigSubstitute(conf, pat, FcMatchPattern);
  FcDefaultSubstitute(pat);

  fs = FcFontSetCreate();
  os = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_FILE, (char *)0);

  FcFontSet *font_patterns;
  font_patterns = FcFontSort(conf, pat, FcTrue, 0, &result);

  if (!font_patterns || font_patterns->nfont == 0) {
    std::cerr << "Fontconfig could not find ANY fonts on the system?"
              << std::endl;
    return 3;
  }

  FcPattern *font_pattern;
  font_pattern = FcFontRenderPrepare(conf, pat, font_patterns->fonts[0]);
  if (font_pattern) {
    FcFontSetAdd(fs, font_pattern);
  } else {
    std::cerr << "Could not prepare matched font for loading." << std::endl;
    return 4;
  }

  FcFontSetSortDestroy(font_patterns);
  FcPatternDestroy(pat);

  if (fs) {
    if (fs->nfont > 0) {
      FcValue v;
      FcPattern *font;

      font = FcPatternFilter(fs->fonts[0], os);

      FcPatternGet(font, FC_FILE, 0, &v);
      const char *filepath = (char *)v.u.f;
      std::cout << query << " path: " << filepath << std::endl;

      FcPatternDestroy(font);
    }
    FcFontSetDestroy(fs);
  } else {
    std::cerr << "could not obtain fs" << std::endl;
  }

  if (os)
    FcObjectSetDestroy(os);

  return 0;
}

static void ensure_env_vars() {
  static const char *pdf2htmlEX_data_dir = getenv("PDF2HTMLEX_DATA_DIR");
  if (nullptr == pdf2htmlEX_data_dir) {
    pdf2htmlEX_data_dir = PDF2HTMLEX_DATA_DIR;
    setenv("PDF2HTMLEX_DATA_DIR", pdf2htmlEX_data_dir, 0);
    std::cout << "PDF2HTMLEX_DATA_DIR set to " << getenv("PDF2HTMLEX_DATA_DIR")
              << std::endl;
  }

  static const char *poppler_data_dir = getenv("POPPLER_DATA_DIR");
  if (nullptr == poppler_data_dir) {
    poppler_data_dir = POPPLER_DATA_DIR;
    setenv("POPPLER_DATA_DIR", poppler_data_dir, 0);
    std::cout << "POPPLER_DATA_DIR set to " << getenv("POPPLER_DATA_DIR")
              << std::endl;
  }

  static const char *fontconfig_path = getenv("FONTCONFIG_PATH");
  if (nullptr == fontconfig_path) {
    fontconfig_path = FONTCONFIG_PATH;
    setenv("FONTCONFIG_PATH", fontconfig_path, 0);
    std::cout << "FONTCONFIG_PATH set to " << getenv("FONTCONFIG_PATH")
              << std::endl;

    fc_test("Helvetica");
  }
}

Html html::pdf2htmlEX_wrapper(const std::string &input_path,
                              const std::string &output_path,
                              const HtmlConfig &config,
                              std::optional<std::string> &password) {
  ensure_env_vars();

  pdf2htmlEX::pdf2htmlEX pdf2htmlEX;

  pdf2htmlEX.setInputFilename(input_path);
  pdf2htmlEX.setDestinationDir(output_path);
  auto output_file_name = "document.html";
  pdf2htmlEX.setOutputFilename(output_file_name);

  pdf2htmlEX.setDRM(false);
  pdf2htmlEX.setProcessOutline(false);
  pdf2htmlEX.setProcessAnnotation(true);

  if (password.has_value()) {
    pdf2htmlEX.setOwnerPassword(password.value());
    pdf2htmlEX.setUserPassword(password.value());
  }

  try {
    pdf2htmlEX.convert();
  } catch (const pdf2htmlEX::EncryptionPasswordException &e) {
    throw WrongPassword();
  } catch (const pdf2htmlEX::DocumentCopyProtectedException &e) {
    throw std::runtime_error("document is copy protected");
  } catch (const pdf2htmlEX::ConversionFailedException &e) {
    throw std::runtime_error(std::string("conversion error ") + e.what());
  }

  return {FileType::portable_document_format,
          config,
          {{"document", output_path + "/" + output_file_name}}};
}

} // namespace odr::internal
