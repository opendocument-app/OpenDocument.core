#ifndef ODR_DOCUMENT_H
#define ODR_DOCUMENT_H

#include <memory>
#include <optional>
#include <string>

namespace odr::internal::common {
class Document;
}

namespace odr {
enum class FileType;
struct FileMeta;
struct Config;

class Document final {
public:
  static std::string version() noexcept;
  static std::string commit() noexcept;

  static FileType type(const std::string &path);
  static FileMeta meta(const std::string &path);

  explicit Document(const std::string &path);
  Document(const std::string &path, FileType as);
  Document(const Document &) = delete;
  Document(Document &&) noexcept;
  ~Document();
  Document &operator=(Document &) = delete;
  Document &operator=(Document &&) noexcept;

  [[nodiscard]] FileType type() const noexcept;
  [[nodiscard]] bool encrypted() const noexcept;
  [[nodiscard]] const FileMeta &meta() const noexcept;

  [[nodiscard]] bool decrypted() const noexcept;
  [[nodiscard]] bool translatable() const noexcept;
  [[nodiscard]] bool editable() const noexcept;
  [[nodiscard]] bool savable(bool encrypted = false) const noexcept;

  [[nodiscard]] bool decrypt(const std::string &password) const;

  void translate(const std::string &path, const Config &config) const;
  void edit(const std::string &diff) const;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

private:
  std::unique_ptr<common::Document> m_impl;
};

class DocumentNoExcept final {
public:
  static std::optional<DocumentNoExcept> open(const std::string &path) noexcept;
  static std::optional<DocumentNoExcept> open(const std::string &path,
                                              FileType as) noexcept;

  static FileType type(const std::string &path) noexcept;
  static FileMeta meta(const std::string &path) noexcept;

  explicit DocumentNoExcept(std::unique_ptr<Document>);

  [[nodiscard]] FileType type() const noexcept;
  [[nodiscard]] bool encrypted() const noexcept;
  [[nodiscard]] const FileMeta &meta() const noexcept;

  [[nodiscard]] bool decrypted() const noexcept;
  [[nodiscard]] bool translatable() const noexcept;
  [[nodiscard]] bool editable() const noexcept;
  [[nodiscard]] bool savable(bool encrypted = false) const noexcept;

  [[nodiscard]] bool decrypt(const std::string &password) const noexcept;

  [[nodiscard]] bool translate(const std::string &path,
                               const Config &config) const noexcept;
  [[nodiscard]] bool edit(const std::string &diff) const noexcept;

  [[nodiscard]] bool save(const std::string &path) const noexcept;
  [[nodiscard]] bool save(const std::string &path,
                          const std::string &password) const noexcept;

private:
  std::unique_ptr<Document> m_impl;
};

} // namespace odr

#endif // ODR_DOCUMENT_H
