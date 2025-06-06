#pragma once

#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/style.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace odr {
class Archive;
struct HtmlPage;
class HtmlService;
class HtmlResource;
struct HtmlConfig;

using HtmlResourceLocation = std::optional<std::string>;
using HtmlResourceLocator = std::function<HtmlResourceLocation(
    const HtmlResource &resource, const HtmlConfig &config)>;

/// @brief HTML table gridlines.
enum class HtmlTableGridlines {
  none,
  soft,
  hard,
};

/// @brief HTML configuration.
struct HtmlConfig {
  // document output file names
  std::string document_output_file_name{"document.html"};

  // document element output file names
  std::string slide_output_file_name{"slide{index}.html"};
  std::string sheet_output_file_name{"sheet{index}.html"};
  std::string page_output_file_name{"page{index}.html"};

  // embedding
  bool embed_images{true};
  bool embed_shipped_resources{true};

  // resources
  std::string resource_path;
  bool relative_resource_paths{true};

  // create editable output
  bool editable{false};

  // text document margin
  bool text_document_margin{false};

  // spreadsheet table limit
  std::optional<TableDimensions> spreadsheet_limit{TableDimensions(10000, 500)};
  bool spreadsheet_limit_by_content{true};
  // spreadsheet gridlines
  HtmlTableGridlines spreadsheet_gridlines{HtmlTableGridlines::soft};

  // formatting
  bool format_html{false};
  std::uint8_t html_indent{2};

  std::optional<std::string> output_path;
  HtmlResourceLocator resource_locator;

  HtmlConfig();
  explicit HtmlConfig(std::string output_path);

private:
  void init();
};

/// @brief HTML output.
///
/// Represents the output of a translated file to HTML.
class Html final {
public:
  Html(HtmlConfig config, std::vector<HtmlPage> pages);

  [[nodiscard]] const HtmlConfig &config();
  [[nodiscard]] const std::vector<HtmlPage> &pages() const;

private:
  HtmlConfig m_config;
  std::vector<HtmlPage> m_pages;
};

/// @brief HTML page.
///
/// Captures the name and path of an HTML page.
struct HtmlPage final {
  std::string name;
  std::string path;

  HtmlPage(std::string name, std::string path);
};

namespace html {

HtmlResourceLocator standard_resource_locator();

/// @brief Translates a decoded file to HTML.
///
/// @param file Decoded file to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @return HTML output.
HtmlService translate(const DecodedFile &file, const std::string &cache_path,
                      const HtmlConfig &config);

/// @brief Translates a text file to HTML.
///
/// @param text_file Text file to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @return HTML output.
HtmlService translate(const TextFile &text_file, const std::string &cache_path,
                      const HtmlConfig &config);
/// @brief Translates an image file to HTML.
///
/// @param image_file Image file to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @return HTML output.
HtmlService translate(const ImageFile &image_file,
                      const std::string &cache_path, const HtmlConfig &config);
/// @brief Translates an archive to HTML.
///
/// @param archive Archive file to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @return HTML output.
HtmlService translate(const ArchiveFile &archive_file,
                      const std::string &cache_path, const HtmlConfig &config);
/// @brief Translates a document to HTML.
///
/// @param document_file Document file to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @return HTML output.
HtmlService translate(const DocumentFile &document_file,
                      const std::string &cache_path, const HtmlConfig &config);
/// @brief Translates a PDF file to HTML.
///
/// @param pdf_file PDF file to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @return HTML output.
HtmlService translate(const PdfFile &pdf_file, const std::string &cache_path,
                      const HtmlConfig &config);

/// @brief Translates an archive to HTML.
///
/// @param archive Archive to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @return HTML output.
HtmlService translate(const Archive &archive, const std::string &cache_path,
                      const HtmlConfig &config);
/// @brief Translates a document to HTML.
///
/// @param document Document to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @return HTML output.
HtmlService translate(const Document &document, const std::string &cache_path,
                      const HtmlConfig &config);

/// @brief Edits a document with a diff.
///
/// @note The diff is generated by our JavaScript code in the browser.
///
/// @param document Document to edit.
/// @param diff Diff to apply.
void edit(const Document &document, const char *diff);

} // namespace html

} // namespace odr
