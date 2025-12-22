#pragma once

#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/style.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace odr::internal::abstract {
class HtmlService;
class HtmlView;
class HtmlResource;
} // namespace odr::internal::abstract

namespace odr {
class Archive;
struct HtmlPage;
class HtmlService;
struct HtmlConfig;

enum class HtmlResourceType {
  html_fragment,
  css,
  js,
  image,
  font,
};

class HtmlResource final {
public:
  HtmlResource();
  explicit HtmlResource(std::shared_ptr<internal::abstract::HtmlResource> impl);

  [[nodiscard]] HtmlResourceType type() const;
  [[nodiscard]] const std::string &mime_type() const;
  [[nodiscard]] const std::string &name() const;
  [[nodiscard]] const std::string &path() const;
  [[nodiscard]] const std::optional<File> &file() const;
  [[nodiscard]] bool is_shipped() const;
  [[nodiscard]] bool is_external() const;
  [[nodiscard]] bool is_accessible() const;

  void write_resource(std::ostream &os) const;

private:
  std::shared_ptr<internal::abstract::HtmlResource> m_impl;
};

using HtmlResourceLocation = std::optional<std::string>;
using HtmlResourceLocator = std::function<HtmlResourceLocation(
    const HtmlResource &resource, const HtmlConfig &config)>;
using HtmlResources =
    std::vector<std::pair<HtmlResource, HtmlResourceLocation>>;

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

  // background image
  std::string background_image_format{"png"};

  // drm
  bool no_drm{false};

  std::optional<std::string> output_path;
  HtmlResourceLocator resource_locator;

  HtmlConfig();
  explicit HtmlConfig(std::string output_path_);

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

class HtmlView final {
public:
  HtmlView();
  explicit HtmlView(std::shared_ptr<internal::abstract::HtmlView> impl);

  [[nodiscard]] const std::string &name() const;
  [[nodiscard]] const std::size_t index() const;
  [[nodiscard]] const std::string &path() const;
  [[nodiscard]] const HtmlConfig &config() const;

  HtmlResources write_html(std::ostream &out) const;

  [[nodiscard]] Html bring_offline(const std::string &output_path) const;

  [[nodiscard]] const std::shared_ptr<internal::abstract::HtmlView> &
  impl() const;

private:
  std::shared_ptr<internal::abstract::HtmlView> m_impl;
};

using HtmlViews = std::vector<HtmlView>;

class HtmlService final {
public:
  HtmlService();
  explicit HtmlService(std::shared_ptr<internal::abstract::HtmlService> impl);

  [[nodiscard]] const HtmlConfig &config() const;
  [[nodiscard]] const HtmlViews &list_views() const;

  void warmup() const;

  [[nodiscard]] bool exists(const std::string &path) const;
  [[nodiscard]] std::string mimetype(const std::string &path) const;

  void write(const std::string &path, std::ostream &out) const;
  HtmlResources write_html(const std::string &path, std::ostream &out) const;

  [[nodiscard]] Html bring_offline(const std::string &output_path) const;
  [[nodiscard]] Html bring_offline(const std::string &output_path,
                                   const std::vector<HtmlView> &views) const;

  [[nodiscard]] const std::shared_ptr<internal::abstract::HtmlService> &
  impl() const;

private:
  std::shared_ptr<internal::abstract::HtmlService> m_impl;
};

namespace html {

HtmlResourceLocator standard_resource_locator();

/// @brief Translates a decoded file to HTML.
///
/// @param file Decoded file to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @param logger Logger to use for logging.
/// @return HTML output.
HtmlService translate(const DecodedFile &file, const std::string &cache_path,
                      const HtmlConfig &config,
                      std::shared_ptr<Logger> logger = Logger::create_null());

/// @brief Translates a text file to HTML.
///
/// @param text_file Text file to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @param logger Logger to use for logging.
/// @return HTML output.
HtmlService translate(const TextFile &text_file, const std::string &cache_path,
                      const HtmlConfig &config,
                      std::shared_ptr<Logger> logger = Logger::create_null());
/// @brief Translates an image file to HTML.
///
/// @param image_file Image file to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @param logger Logger to use for logging.
/// @return HTML output.
HtmlService translate(const ImageFile &image_file,
                      const std::string &cache_path, const HtmlConfig &config,
                      std::shared_ptr<Logger> logger = Logger::create_null());
/// @brief Translates an archive to HTML.
///
/// @param archive_file Archive file to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @param logger Logger to use for logging.
/// @return HTML output.
HtmlService translate(const ArchiveFile &archive_file,
                      const std::string &cache_path, const HtmlConfig &config,
                      std::shared_ptr<Logger> logger = Logger::create_null());
/// @brief Translates a document to HTML.
///
/// @param document_file Document file to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @param logger Logger to use for logging.
/// @return HTML output.
HtmlService translate(const DocumentFile &document_file,
                      const std::string &cache_path, const HtmlConfig &config,
                      std::shared_ptr<Logger> logger = Logger::create_null());
/// @brief Translates a PDF file to HTML.
///
/// @param pdf_file PDF file to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @param logger Logger to use for logging.
/// @return HTML output.
HtmlService translate(const PdfFile &pdf_file, const std::string &cache_path,
                      const HtmlConfig &config,
                      std::shared_ptr<Logger> logger = Logger::create_null());

/// @brief Translates an archive to HTML.
///
/// @param archive Archive to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @param logger Logger to use for logging.
/// @return HTML output.
HtmlService translate(const Archive &archive, const std::string &cache_path,
                      const HtmlConfig &config,
                      std::shared_ptr<Logger> logger = Logger::create_null());
/// @brief Translates a document to HTML.
///
/// @param document Document to translate.
/// @param cache_path Directory path for temporary output.
/// @param config Configuration for the HTML output.
/// @param logger Logger to use for logging.
/// @return HTML output.
HtmlService translate(const Document &document, const std::string &cache_path,
                      const HtmlConfig &config,
                      std::shared_ptr<Logger> logger = Logger::create_null());

/// @brief Edits a document with a diff.
///
/// @note The diff is generated by our JavaScript code in the browser.
///
/// @param document Document to edit.
/// @param diff Diff to apply.
/// @param logger Logger to use for logging.
void edit(const Document &document, const char *diff,
          Logger &logger = Logger::null());

} // namespace html

} // namespace odr
