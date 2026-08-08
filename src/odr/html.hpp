#pragma once

#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/table_dimension.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::abstract {
class HtmlService;
class HtmlView;
class HtmlResource;
} // namespace odr::internal::abstract

namespace odr {
class Archive;
class Filesystem;
struct HtmlPage;
class HtmlService;
struct HtmlConfig;

enum class HtmlResourceType {
  html_fragment,
  css,
  js,
  image,
  font,
  // appended rather than sorted in: the bindings mirror this enum by ordinal
  /// Audio or video, never embedded — see @ref HtmlConfig::embed_images.
  media,
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
  /// @deprecated Inert: always false. The css and js are written into the
  /// document, so no resource is shipped alongside it any more.
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

/// @brief Initial zoom of the emitted HTML on mobile (viewport meta tag).
/// Desktop browsers ignore the tag entirely.
enum class HtmlViewportMode {
  automatic,   ///< `fit_width` for fixed-size paged content (PDF pages, slides,
               ///< drawings, images, text documents with page margins),
               ///< `actual_size` for reflowing content (spreadsheets, text)
  fit_width,   ///< initial zoom fits the content's full width on screen
  actual_size, ///< initial zoom locked to 100% (`initial-scale=1.0`)
  none,        ///< no viewport meta tag at all
};

/// @brief How text is emitted in PDF→HTML output. Neither mode needs
/// JavaScript.
enum class PdfTextMode {
  dual_layer,   ///< a visual layer (paint order, embedded PUA glyphs) plus a
                ///< transparent selection layer (reading order, real Unicode),
                ///< like pdf.js
  single_layer, ///< one layer, every glyph mapped to Unicode by frequency
                ///< analysis, like pdf2htmlEX
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
  /// @deprecated Inert: nothing is shipped any more, the css and js are
  /// written into the document.
  bool embed_shipped_resources{true};

  // resources
  /// @deprecated See @ref embed_shipped_resources.
  std::string resource_path;
  /// @deprecated See @ref embed_shipped_resources.
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

  // initial zoom on mobile
  HtmlViewportMode viewport_mode{HtmlViewportMode::automatic};
  // overrides `viewport_mode` for spreadsheet content when set
  std::optional<HtmlViewportMode> spreadsheet_viewport_mode;
  // raw `content` for the viewport meta tag; overrides the modes above
  std::optional<std::string> viewport_content;

  // formatting
  bool format_html{false};
  // Indentation when `format_html` is set: `html_indent_string` is repeated
  // `html_indent` times per nesting level (0 disables indentation entirely).
  std::uint8_t html_indent{1};
  std::string html_indent_string{"\t"};

  // background image
  std::string background_image_format{"png"};
  double background_image_dpi{144.0};

  // Paged-document page range (currently honored by the PDF pipeline): render
  // only pages with 0-based index in `[page_range_begin, page_range_end)`.
  // Page views and `#pN` anchors keep their document-global page numbers.
  std::uint32_t page_range_begin{0};
  std::optional<std::uint32_t> page_range_end;

  // PDF text mode
  PdfTextMode pdf_text_mode{PdfTextMode::dual_layer};
  // `dual_layer` renders its invisible selection layer in a local system font
  // (first of these that resolves), whose natural width rarely matches the
  // PDF-derived box CSS justify has to fill — and justify can only add spacing.
  // The size-adjust (0-1, written as the @font-face percent) shrinks the
  // fallback's metrics toward the PDF's to close that gap. Safe to
  // underestimate, not to overestimate: the excess is clipped, not shrunk.
  std::vector<std::string> pdf_dual_layer_fallback_fonts{
      "Arial", "Helvetica", "Liberation Sans", "DejaVu Sans", "Nimbus Sans"};
  double pdf_dual_layer_fallback_font_size_adjust{0.5};

  // drm options
  bool no_drm{false};

  // outline options
  bool embed_outline{false};

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

  [[nodiscard]] const HtmlConfig &config() const;
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
  [[nodiscard]] std::size_t index() const;
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
HtmlService translate(const DecodedFile &file, const HtmlConfig &config,
                      const Logger &logger = Logger::null());

/// @brief Translates a text file to HTML.
HtmlService translate(const TextFile &text_file, const HtmlConfig &config,
                      const Logger &logger = Logger::null());
/// @brief Translates an image file to HTML.
HtmlService translate(const ImageFile &image_file, const HtmlConfig &config,
                      const Logger &logger = Logger::null());
/// @brief Translates an archive file to HTML.
HtmlService translate(const ArchiveFile &archive_file, const HtmlConfig &config,
                      const Logger &logger = Logger::null());
/// @brief Translates a document file to HTML.
HtmlService translate(const DocumentFile &document_file,
                      const HtmlConfig &config,
                      const Logger &logger = Logger::null());
/// @brief Translates a PDF file to HTML.
HtmlService translate(const PdfFile &pdf_file, const HtmlConfig &config,
                      const Logger &logger = Logger::null());

/// @brief Translates a font file to HTML (a specimen page).
HtmlService translate(const FontFile &font_file, const HtmlConfig &config,
                      const Logger &logger = Logger::null());

/// @brief Translates a filesystem to HTML.
HtmlService translate(const Filesystem &filesystem, const HtmlConfig &config,
                      const Logger &logger = Logger::null());
/// @brief Translates an archive to HTML.
HtmlService translate(const Archive &archive, const HtmlConfig &config,
                      const Logger &logger = Logger::null());
/// @brief Translates a document to HTML.
HtmlService translate(const Document &document, const HtmlConfig &config,
                      const Logger &logger = Logger::null());

/// @name Translation with a cache path
///
/// `cache_path` is ignored — nothing on the render path writes to disk, and no
/// renderer has read it since the output became a set of streams. Kept so
/// existing callers keep compiling; prefer the overloads above.
/// @{
HtmlService translate(const DecodedFile &file, const std::string &cache_path,
                      const HtmlConfig &config,
                      const Logger &logger = Logger::null());
HtmlService translate(const TextFile &text_file, const std::string &cache_path,
                      const HtmlConfig &config,
                      const Logger &logger = Logger::null());
HtmlService translate(const ImageFile &image_file,
                      const std::string &cache_path, const HtmlConfig &config,
                      const Logger &logger = Logger::null());
HtmlService translate(const ArchiveFile &archive_file,
                      const std::string &cache_path, const HtmlConfig &config,
                      const Logger &logger = Logger::null());
HtmlService translate(const DocumentFile &document_file,
                      const std::string &cache_path, const HtmlConfig &config,
                      const Logger &logger = Logger::null());
HtmlService translate(const PdfFile &pdf_file, const std::string &cache_path,
                      const HtmlConfig &config,
                      const Logger &logger = Logger::null());
HtmlService translate(const FontFile &font_file, const std::string &cache_path,
                      const HtmlConfig &config,
                      const Logger &logger = Logger::null());
HtmlService translate(const Filesystem &filesystem,
                      const std::string &cache_path, const HtmlConfig &config,
                      const Logger &logger = Logger::null());
HtmlService translate(const Archive &archive, const std::string &cache_path,
                      const HtmlConfig &config,
                      const Logger &logger = Logger::null());
HtmlService translate(const Document &document, const std::string &cache_path,
                      const HtmlConfig &config,
                      const Logger &logger = Logger::null());
/// @}

/// @brief Applies a diff to a document. The diff is what our JavaScript
/// produces in the browser.
void edit(const Document &document, std::string_view diff,
          const Logger &logger = Logger::null());

} // namespace html

} // namespace odr
