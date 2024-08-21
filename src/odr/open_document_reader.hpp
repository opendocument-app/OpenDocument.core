#ifndef ODR_OPEN_DOCUMENT_READER_HPP
#define ODR_OPEN_DOCUMENT_READER_HPP

#include <functional>
#include <odr/internal/project_info.hpp>
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

/// @brief Callback to get the password for encrypted files.
using PasswordCallback = std::function<std::string()>;

/// @brief Main entry point for the Open Document Reader library.
class OpenDocumentReader final {
public:
  /// @brief Get the version of the Open Document Reader library.
  /// @return The version of the Open Document Reader library.
  [[nodiscard]] static std::string version() noexcept;
  /// @brief Get the commit hash of the Open Document Reader library.
  /// @return The commit hash of the Open Document Reader library.
  [[nodiscard]] static std::string commit() noexcept;

  /// @brief Get the file type by the file extension.
  /// @param extension The file extension.
  /// @return The file type.
  [[nodiscard]] static FileType
  type_by_extension(const std::string &extension) noexcept;
  /// @brief Get the file category by the file type.
  /// @param type The file type.
  /// @return The file category.
  [[nodiscard]] static FileCategory category_by_type(FileType type) noexcept;
  /// @brief Get the file type as a string.
  /// @param type The file type.
  /// @return The file type as a string.
  [[nodiscard]] static std::string type_to_string(FileType type) noexcept;

  /// @brief Get the file types by the file path.
  /// @param path The file path.
  /// @return The file types.
  [[nodiscard]] static std::vector<FileType> types(const std::string &path);
  /// @brief Open a file.
  /// @param path The file path.
  /// @return The decoded file.
  [[nodiscard]] static DecodedFile open(const std::string &path);

  /// @brief Translates a file to HTML.
  ///
  /// @deprecated This function is deprecated because it hides API which can be
  /// useful to the user like inspecting if a file is encrypted and if the
  /// password was wrong. Use `html(const DecodedFile &, const std::string &,
  /// const HtmlConfig &)` instead.
  ///
  /// @param input_path Path to the file to translate.
  /// @param output_path Path to save the HTML output.
  /// @param config Configuration for the HTML output.
  /// @param password_callback Callback to get the password for encrypted files.
  /// @return HTML output.
  [[deprecated]] [[nodiscard]] static Html
  html(const std::string &input_path, const PasswordCallback &password_callback,
       const std::string &output_path, const HtmlConfig &config);
  /// @brief Translates a file to HTML.
  ///
  /// @deprecated This function is deprecated because it hides API which can be
  /// useful to the user like inspecting if a file is encrypted and if the
  /// password was wrong. Use `html(const DecodedFile &, const std::string &,
  /// const HtmlConfig &)` instead.
  ///
  /// @param file File to translate.
  /// @param output_path Path to save the HTML output.
  /// @param config Configuration for the HTML output.
  /// @param password_callback Callback to get the password for encrypted files.
  /// @return HTML output.
  [[deprecated]] [[nodiscard]] static Html
  html(const File &file, const PasswordCallback &password_callback,
       const std::string &output_path, const HtmlConfig &config);
  /// @brief Translates a decoded file to HTML.
  ///
  /// @param file Decoded file to translate.
  /// @param output_path Path to save the HTML output.
  /// @param config Configuration for the HTML output.
  /// @return HTML output.
  [[nodiscard]] static Html html(const DecodedFile &file,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  /// @brief Translates a text file to HTML.
  ///
  /// @param text_file Text file to translate.
  /// @param output_path Path to save the HTML output.
  /// @param config Configuration for the HTML output.
  /// @return HTML output.
  [[nodiscard]] static Html html(const TextFile &text_file,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  /// @brief Translates an image file to HTML.
  ///
  /// @param image_file Image file to translate.
  /// @param output_path Path to save the HTML output.
  /// @param config Configuration for the HTML output.
  /// @return HTML output.
  [[nodiscard]] static Html html(const ImageFile &image_file,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  /// @brief Translates an archive to HTML.
  ///
  /// @param archive Archive to translate.
  /// @param output_path Path to save the HTML output.
  /// @param config Configuration for the HTML output.
  /// @return HTML output.
  [[nodiscard]] static Html html(const Archive &archive,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  /// @brief Translates a document to HTML.
  ///
  /// @param document Document to translate.
  /// @param output_path Path to save the HTML output.
  /// @param config Configuration for the HTML output.
  /// @return HTML output.
  [[nodiscard]] static Html html(const Document &document,
                                 const std::string &output_path,
                                 const HtmlConfig &config);
  /// @brief Translates a PDF file to HTML.
  ///
  /// @param pdf_file PDF file to translate.
  /// @param output_path Path to save the HTML output.
  /// @param config Configuration for the HTML output.
  /// @return HTML output.
  [[nodiscard]] static Html html(const PdfFile &pdf_file,
                                 const std::string &output_path,
                                 const HtmlConfig &config);

#if defined(WITH_PDF2HTMLEX)
  /// @brief Translates a PDF file to HTML using pdf2htmlEX.
  ///
  /// @param pdf_file PDF file to translate.
  /// @param output_path Path to save the HTML output.
  /// @param config Configuration for the HTML output.
  /// @return HTML output.
  [[nodiscard]] static Html pdf2htmlEX(const PdfFile &pdf_file,
                                       const std::string &output_path,
                                       const HtmlConfig &config);
#endif

#if defined(WITH_WVWARE)
  /// @brief Translates a file to HTML.
  ///
  /// @param file File to translate.
  /// @param output_path Path to save the HTML output.
  /// @param config Configuration for the HTML output.
  /// @return HTML output.
  [[nodiscard]] static Html wvHtml(const File &file,
                                   const std::string &output_path,
                                   const HtmlConfig &config);
#endif

  /// @brief Edit a document.
  /// @param document The document.
  /// @param diff The diff.
  void edit(const Document &document, const char *diff);

  /// @brief Copy the resources to the specified path.
  /// @param to_path The path to copy the resources to.
  static void copy_resources(const std::string &to_path);

private:
  OpenDocumentReader();
};

} // namespace odr

#endif // ODR_OPEN_DOCUMENT_READER_HPP
