#ifndef ODR_OPEN_DOCUMENT_READER_H
#define ODR_OPEN_DOCUMENT_READER_H

#include <optional>
#include <string>

namespace odr {
struct HtmlConfig;
class Document;
class Html;
class HtmlPage;

class OpenDocumentReader final {
public:
  [[nodiscard]] static std::string version() noexcept;
  [[nodiscard]] static std::string commit() noexcept;

  static Html html(const std::string &path, const char *password,
                   const HtmlConfig &config);

private:
  OpenDocumentReader();
};

class Html final {
public:
  const std::vector<HtmlPage> &pages() const;

  void edit(const char *diff);
  void save(const std::string &path) const;

private:
  Html(FileType file_type, HtmlConfig config, std::vector<HtmlPage> pages,
       Document document);

  FileType m_file_type;
  HtmlConfig m_config;
  std::vector<HtmlPage> m_pages;
  std::optional<Document> m_document;

  friend class OpenDocumentReader;
};

class HtmlPage final {
public:
  const std::string &name() const;
  const std::string &path() const;

private:
  HtmlPage(std::string name, std::string path);

  std::string m_name;
  std::string m_path;
};

} // namespace odr

#endif // ODR_OPEN_DOCUMENT_READER_H
