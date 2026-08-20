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
  /// An entry of an archive, offered as itself rather than rendered.
  file,
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
  /// One of the renderer's own compiled-in css/js rather than something the
  /// document carries. See @ref HtmlConfig::embed_shipped_resources.
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

/// @brief The colors the emitted HTML renders against. @ref
/// FileTypeCapabilities::color_scheme says which views honor it.
enum class HtmlColorScheme {
  light,  ///< a white page, carrying the colors the document gives its content
  dark,   ///< a dark page, which the document's own colors give way to
  system, ///< `light` or `dark`, by the reader's `prefers-color-scheme`
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
  /// File name for the view that writes the whole document.
  std::string document_output_file_name{"document.html"};

  // per-element view file names; `{index}` is the element's 0-based number
  std::string slide_output_file_name{"slide{index}.html"};
  std::string sheet_output_file_name{"sheet{index}.html"};
  std::string page_output_file_name{"page{index}.html"};

  /// Embed images as data urls rather than writing them beside the document.
  bool embed_images{true};
  /// Write the renderer's own css and js into every document rather than beside
  /// it as one shared file the documents link.
  bool embed_shipped_resources{true};

  /// Where linked shipped resources go, relative to the output path unless
  /// named absolutely. Empty puts them beside the document.
  std::string resource_path;
  /// Link an absolute @ref resource_path relative to the document, so the
  /// output stays movable.
  bool relative_resource_paths{true};

  /// Write `contenteditable` output, which back-translation reads edits from.
  bool editable{false};

  /// Render a text document as fixed-size pages rather than reflowing text.
  bool text_document_margin{false};

  /// The colors the output renders against.
  HtmlColorScheme color_scheme{HtmlColorScheme::light};

  /// Largest sheet region written; cells past it are dropped.
  std::optional<TableDimensions> spreadsheet_limit{TableDimensions(10000, 500)};
  /// Trim a sheet to the cells it uses before @ref spreadsheet_limit applies.
  bool spreadsheet_limit_by_content{true};
  /// Which gridlines a sheet paints.
  HtmlTableGridlines spreadsheet_gridlines{HtmlTableGridlines::soft};

  /// Initial zoom on mobile; see @ref HtmlViewportMode.
  HtmlViewportMode viewport_mode{HtmlViewportMode::automatic};
  /// Overrides @ref viewport_mode for spreadsheet content when set.
  std::optional<HtmlViewportMode> spreadsheet_viewport_mode;
  /// Raw `content` for the viewport meta tag; overrides the modes above.
  std::optional<std::string> viewport_content;
  /// The width the output is shown at, in css pixels; fits paged content to it.
  std::optional<std::uint32_t> viewport_width;

  /// Indent and break the output into lines rather than writing one stream.
  bool format_html{false};
  /// Repeated @ref html_indent_string per nesting level; 0 disables indenting.
  std::uint8_t html_indent{1};
  std::string html_indent_string{"\t"};

  /// @deprecated Inert: no view renders a background image to a file.
  std::string background_image_format{"png"};
  /// @deprecated See @ref background_image_format.
  double background_image_dpi{144.0};

  /// Renders only the pages with 0-based index in `[page_range_begin,
  /// page_range_end)`; page views and `#pN` anchors keep their document-global
  /// numbers. Honored by the pdf pipeline.
  std::uint32_t page_range_begin{0};
  std::optional<std::uint32_t> page_range_end;

  /// How pdf text is written; see @ref PdfTextMode.
  PdfTextMode pdf_text_mode{PdfTextMode::dual_layer};
  /// System fonts `dual_layer` sets its selection layer in, first that
  /// resolves.
  std::vector<std::string> pdf_dual_layer_fallback_fonts{
      "Arial", "Helvetica", "Liberation Sans", "DejaVu Sans", "Nimbus Sans"};
  /// Shrinks the fallback's metrics toward the pdf's (0-1) so css justify can
  /// fill the box. Safe to underestimate: the excess is clipped, not shrunk.
  double pdf_dual_layer_fallback_font_size_adjust{0.5};

  /// @deprecated Inert: no output carries a restriction to lift.
  bool no_drm{false};

  /// @deprecated Inert: an outline is never written.
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
