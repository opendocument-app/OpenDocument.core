#ifndef ODR_OPEN_DOCUMENT_READER_H
#define ODR_OPEN_DOCUMENT_READER_H

#include <string>

namespace odr {
class Html;
struct HtmlConfig;
class Document;

class OpenDocumentReader final {
public:
  [[nodiscard]] static std::string version() noexcept;
  [[nodiscard]] static std::string commit() noexcept;

  [[nodiscard]] static Html html(const std::string &input_path,
                                 const char *password,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  [[nodiscard]] static Html html(Document document,
                                 const std::string &output_path,
                                 const HtmlConfig &config);

private:
  OpenDocumentReader();
};

} // namespace odr

#endif // ODR_OPEN_DOCUMENT_READER_H
