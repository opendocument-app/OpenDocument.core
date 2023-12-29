#ifndef ODR_OPEN_DOCUMENT_READER_HPP
#define ODR_OPEN_DOCUMENT_READER_HPP

#include <string>
#include <vector>

namespace odr {
enum class FileType;
enum class FileCategory;
class DecodedFile;
class TextFile;
class Document;
class Html;
struct HtmlConfig;

class OpenDocumentReader final {
public:
  [[nodiscard]] static std::string version() noexcept;
  [[nodiscard]] static std::string commit() noexcept;

  [[nodiscard]] static FileType
  type_by_extension(const std::string &extension) noexcept;
  [[nodiscard]] static FileCategory category_by_type(FileType type) noexcept;
  [[nodiscard]] static std::string type_to_string(FileType type) noexcept;

  [[nodiscard]] static std::vector<FileType> types(const std::string &path);
  [[nodiscard]] static DecodedFile open(const std::string &path);

  [[nodiscard]] static Html html(const std::string &input_path,
                                 const char *password,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  [[nodiscard]] static Html html(const DecodedFile &file, const char *password,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  [[nodiscard]] static Html html(const TextFile &text_file,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  [[nodiscard]] static Html html(const Document &document,
                                 const std::string &output_path,
                                 const HtmlConfig &config);

  static void copy_resources(const std::string &to_path);

private:
  OpenDocumentReader();
};

} // namespace odr

#endif // ODR_OPEN_DOCUMENT_READER_HPP
