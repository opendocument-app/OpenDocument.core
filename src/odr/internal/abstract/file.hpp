#pragma once

#include <odr/file.hpp>

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace odr::internal {
class AbsPath;
}

namespace odr::internal::abstract {
class Image;
class Archive;
class Document;
class Font;

class File {
public:
  virtual ~File() = default;

  [[nodiscard]] virtual FileLocation location() const noexcept = 0;
  [[nodiscard]] virtual std::size_t size() const = 0;

  [[nodiscard]] virtual std::optional<AbsPath> disk_path() const = 0;
  /// The file's bytes if it is held in memory, else nullopt.
  [[nodiscard]] virtual std::optional<std::string_view> memory_data() const = 0;

  [[nodiscard]] virtual std::unique_ptr<std::istream> stream() const = 0;
};

class DecodedFile {
public:
  virtual ~DecodedFile() = default;

  [[nodiscard]] virtual std::shared_ptr<File> file() const noexcept = 0;

  [[nodiscard]] virtual FileType file_type() const noexcept = 0;
  [[nodiscard]] virtual FileCategory file_category() const noexcept = 0;
  [[nodiscard]] virtual std::string_view mimetype() const noexcept = 0;
  [[nodiscard]] virtual FileMeta file_meta() const noexcept = 0;

  [[nodiscard]] virtual bool password_encrypted() const noexcept {
    return false;
  }
  [[nodiscard]] virtual EncryptionState encryption_state() const noexcept {
    return EncryptionState::not_encrypted;
  }
  [[nodiscard]] virtual std::shared_ptr<DecodedFile>
  decrypt([[maybe_unused]] const std::string &password) const {
    return nullptr;
  }

  [[nodiscard]] virtual bool is_decodable() const noexcept = 0;
};

class TextFile : public DecodedFile {
public:
  [[nodiscard]] FileCategory file_category() const noexcept final {
    return FileCategory::text;
  }

  /// The encoding the bytes were detected as, or decoded with.
  [[nodiscard]] virtual TextEncoding encoding() const noexcept = 0;
};

class ImageFile : public DecodedFile {
public:
  [[nodiscard]] FileCategory file_category() const noexcept final {
    return FileCategory::image;
  }

  [[nodiscard]] virtual std::shared_ptr<Image> image() const = 0;
};

class ArchiveFile : public DecodedFile {
public:
  [[nodiscard]] FileCategory file_category() const noexcept final {
    return FileCategory::archive;
  }

  [[nodiscard]] virtual std::shared_ptr<Archive> archive() const = 0;
};

class DocumentFile : public DecodedFile {
public:
  [[nodiscard]] FileCategory file_category() const noexcept final {
    return FileCategory::document;
  }

  [[nodiscard]] virtual DocumentType document_type() const = 0;

  [[nodiscard]] virtual std::shared_ptr<Document> document() const = 0;
};

/// A csv is a text file that can also be loaded as a document — a one-sheet
/// spreadsheet. It stays in @ref FileCategory::text, so reading it as text
/// needs no reopening; @ref document is the other view of the same bytes.
class CsvFile : public TextFile {
public:
  /// The options in use, every field resolved.
  [[nodiscard]] virtual CsvOptions options() const = 0;

  /// The same file read with @p options.
  [[nodiscard]] virtual std::shared_ptr<CsvFile>
  with_options(const CsvOptions &options) const = 0;

  /// The csv as a one-sheet spreadsheet.
  [[nodiscard]] virtual std::shared_ptr<Document> document() const = 0;
};

/// A markdown file is a text file that can also be loaded as a document — its
/// prose, parsed. Like @ref CsvFile it stays in @ref FileCategory::text; @ref
/// document is the other view of the same bytes.
class MarkdownFile : public TextFile {
public:
  /// The markdown as a text document.
  [[nodiscard]] virtual std::shared_ptr<Document> document() const = 0;
};

class PdfFile : public DecodedFile {
public:
  [[nodiscard]] FileType file_type() const noexcept final {
    return FileType::portable_document_format;
  }
  [[nodiscard]] FileCategory file_category() const noexcept final {
    return FileCategory::document;
  }
  [[nodiscard]] std::string_view mimetype() const noexcept final {
    return "application/pdf";
  }
};

class FontFile : public DecodedFile {
public:
  [[nodiscard]] FileCategory file_category() const noexcept final {
    return FileCategory::font;
  }

  [[nodiscard]] virtual std::shared_ptr<Font> font() const = 0;
};

} // namespace odr::internal::abstract
