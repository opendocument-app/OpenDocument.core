#ifndef ODR_OPEN_DOCUMENT_READER_HPP
#define ODR_OPEN_DOCUMENT_READER_HPP

#include <functional>
#include <string>
#include <vector>

namespace odr {
enum class FileType;
enum class FileCategory;
class File;
class DecodedFile;
class TextFile;
class ImageFile;
class Archive;
class Document;
class PdfFile;
class Html;
struct HtmlConfig;

using PasswordCallback = std::function<std::string()>;

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
                                 const PasswordCallback &password_callback,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  [[nodiscard]] static Html html(const File &file,
                                 const PasswordCallback &password_callback,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  [[nodiscard]] static Html html(const DecodedFile &file,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  [[nodiscard]] static Html html(const TextFile &text_file,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  [[nodiscard]] static Html html(const ImageFile &image_file,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  [[nodiscard]] static Html html(const Archive &archive,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  [[nodiscard]] static Html html(const Document &document,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  [[nodiscard]] static Html html(const PdfFile &pdf_file,
                                 const std::string &output_path,
                                 const HtmlConfig &config);

  void edit(const Document &document, const char *diff);

  static void copy_resources(const std::string &to_path);

private:
  OpenDocumentReader();
};

} // namespace odr

#endif // ODR_OPEN_DOCUMENT_READER_HPP
