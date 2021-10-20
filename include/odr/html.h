#ifndef ODR_HTML_H
#define ODR_HTML_H

#include <odr/document.h>
#include <odr/file.h>
#include <odr/style.h>
#include <optional>
#include <string>

namespace odr {
class Document;
class HtmlPage;

enum class HtmlTableGridlines {
  none,
  soft,
  hard,
};

struct HtmlConfig {
  bool compact_presentation{false};
  bool compact_drawing{false};

  // create editable output
  bool editable{false};

  // text document margin
  bool text_document_margin{false};

  // spreadsheet table limit
  std::optional<TableDimensions> spreadsheet_limit{TableDimensions(10000, 500)};
  bool spreadsheet_limit_by_content{true};
  // spreadsheet gridlines
  HtmlTableGridlines spreadsheet_gridlines{HtmlTableGridlines::soft};
};

class Html final {
public:
  Html(FileType file_type, HtmlConfig config, std::vector<HtmlPage> pages,
       Document document);

  FileType file_type() const;
  const std::vector<HtmlPage> &pages() const;

  void edit(const char *diff);
  void save(const std::string &path) const;

private:
  FileType m_file_type;
  HtmlConfig m_config;
  std::vector<HtmlPage> m_pages;
  std::optional<Document> m_document;
};

class HtmlPage final {
public:
  HtmlPage(std::string name, std::string path);

  const std::string &name() const;
  const std::string &path() const;

private:
  std::string m_name;
  std::string m_path;
};

namespace html {
Html translate(const Document &document, const std::string &path,
               const HtmlConfig &config);
void edit(const Document &document, const char *diff);
} // namespace html

} // namespace odr

#endif // ODR_HTML_H
