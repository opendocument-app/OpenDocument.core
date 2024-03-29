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

enum class HtmlTableGridlines {
  none,
  soft,
  hard,
};

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

struct HtmlPage final {
  std::string name;
  std::string path;

  HtmlPage(std::string name, std::string path);
};

using PasswordCallback = std::function<std::string()>;

namespace html {

Html translate(const File &file, const std::string &output_path,
               const HtmlConfig &config,
               const PasswordCallback &password_callback);
Html translate(const DecodedFile &file, const std::string &output_path,
               const HtmlConfig &config);

Html translate(const TextFile &text_file, const std::string &output_path,
               const HtmlConfig &config);
Html translate(const ImageFile &image_file, const std::string &output_path,
               const HtmlConfig &config);
Html translate(const Archive &archive, const std::string &output_path,
               const HtmlConfig &config);
Html translate(const Document &document, const std::string &output_path,
               const HtmlConfig &config);
Html translate(const PdfFile &pdf_file, const std::string &output_path,
               const HtmlConfig &config);

void edit(const Document &document, const char *diff);

} // namespace html

} // namespace odr

#endif // ODR_HTML_HPP
