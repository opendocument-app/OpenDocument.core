#ifndef ODR_HTML_HPP
#define ODR_HTML_HPP

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

/// @brief HTML table gridlines.
enum class HtmlTableGridlines {
  none,
  soft,
  hard,
};

/// @brief HTML configuration.
struct HtmlConfig {
  // TODO implement
  bool compact_presentation{false};
  bool compact_spreadsheet{false};
  bool compact_drawing{false};

  // document output file names
  std::string text_document_output_file_name{"document.html"};
  std::string presentation_output_file_name{"presentation.html"};
  std::string spreadsheet_output_file_name{"spreadsheet.html"};
  std::string drawing_output_file_name{"drawing.html"};

  // document element output file names
  std::string slide_output_file_name{"slide{index}.html"};
  std::string sheet_output_file_name{"sheet{index}.html"};
  std::string page_output_file_name{"page{index}.html"};

  // embedding
  bool embed_resources{true};

  // resources
  std::string external_resource_path;
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
};

/// @brief HTML output.
///
/// Represents the output of a translated file to HTML. Also allows for editing
/// and saving the output.
class Html final {
public:
  Html(FileType file_type, HtmlConfig config, std::vector<HtmlPage> pages);
  Html(FileType file_type, HtmlConfig config, std::vector<HtmlPage> pages,
       Document document);

  [[nodiscard]] FileType file_type() const;
  [[nodiscard]] const std::vector<HtmlPage> &pages() const;

  void edit(const char *diff);
  void save(const std::string &path) const;

private:
  FileType m_file_type;
  HtmlConfig m_config;
  std::vector<HtmlPage> m_pages;
  std::optional<Document> m_document;
};

/// @brief HTML page.
///
/// Captures the name and path of an HTML page.
struct HtmlPage final {
  std::string name;
  std::string path;

  HtmlPage(std::string name, std::string path);
};

/// @brief Callback to get the password for encrypted files.
using PasswordCallback = std::function<std::string()>;

namespace html {

/// @brief Translates a file to HTML.
///
/// @deprecated This function is deprecated because it hides API which can be
/// useful to the user like inspecting if a file is encrypted and if the
/// password was wrong. Use `translate(const DecodedFile &, const std::string &,
/// const HtmlConfig &)` instead.
///
/// @param file File to translate.
/// @param output_path Path to save the HTML output.
/// @param config Configuration for the HTML output.
/// @param password_callback Callback to get the password for encrypted files.
/// @return HTML output.
[[deprecated]] Html translate(const File &file, const std::string &output_path,
                              const HtmlConfig &config,
                              const PasswordCallback &password_callback);
/// @brief Translates a decoded file to HTML.
///
/// @param file Decoded file to translate.
/// @param output_path Path to save the HTML output.
/// @param config Configuration for the HTML output.
/// @return HTML output.
Html translate(const DecodedFile &file, const std::string &output_path,
               const HtmlConfig &config);

/// @brief Translates a text file to HTML.
///
/// @param text_file Text file to translate.
/// @param output_path Path to save the HTML output.
/// @param config Configuration for the HTML output.
/// @return HTML output.
Html translate(const TextFile &text_file, const std::string &output_path,
               const HtmlConfig &config);
/// @brief Translates an image file to HTML.
///
/// @param image_file Image file to translate.
/// @param output_path Path to save the HTML output.
/// @param config Configuration for the HTML output.
/// @return HTML output.
Html translate(const ImageFile &image_file, const std::string &output_path,
               const HtmlConfig &config);
/// @brief Translates an archive to HTML.
///
/// @param archive Archive to translate.
/// @param output_path Path to save the HTML output.
/// @param config Configuration for the HTML output.
/// @return HTML output.
Html translate(const Archive &archive, const std::string &output_path,
               const HtmlConfig &config);
/// @brief Translates a document to HTML.
///
/// @param document Document to translate.
/// @param output_path Path to save the HTML output.
/// @param config Configuration for the HTML output.
/// @return HTML output.
Html translate(const Document &document, const std::string &output_path,
               const HtmlConfig &config);
/// @brief Translates a PDF file to HTML.
///
/// @param pdf_file PDF file to translate.
/// @param output_path Path to save the HTML output.
/// @param config Configuration for the HTML output.
/// @return HTML output.
Html translate(const PdfFile &pdf_file, const std::string &output_path,
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

#endif // ODR_HTML_HPP
