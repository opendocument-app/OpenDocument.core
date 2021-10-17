#ifndef ODR_OPEN_DOCUMENT_READER_H
#define ODR_OPEN_DOCUMENT_READER_H

#include <optional>
#include <string>

namespace odr {
struct HtmlConfig;
class Document;

class OpenDocumentReader {
public:
  [[nodiscard]] static std::string version() noexcept;
  [[nodiscard]] static std::string commit() noexcept;

  static std::optional<Document>
  html(const std::string &path, const char *password, const HtmlConfig &config);
  static void edit(Document &document, const char *diff,
                   const std::string &path);

private:
  OpenDocumentReader() = default;
};

} // namespace odr

#endif // ODR_OPEN_DOCUMENT_READER_H
