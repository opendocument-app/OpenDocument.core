#pragma once

#include <odr/file.hpp>

#include <iosfwd>
#include <memory>
#include <string>

namespace odr::internal {
class AbsPath;
}

namespace odr::internal::abstract {
class Image;
class Archive;
class Document;

class File {
public:
  virtual ~File() = default;

  [[nodiscard]] virtual FileLocation location() const noexcept = 0;
  [[nodiscard]] virtual std::size_t size() const = 0;

  [[nodiscard]] virtual std::optional<AbsPath> disk_path() const = 0;
  [[nodiscard]] virtual const char *memory_data() const = 0;

  [[nodiscard]] virtual std::unique_ptr<std::istream> stream() const = 0;
};

class DecodedFile {
public:
  virtual ~DecodedFile() = default;

  [[nodiscard]] virtual std::shared_ptr<File> file() const noexcept = 0;

  [[nodiscard]] virtual FileType file_type() const noexcept = 0;
  [[nodiscard]] virtual FileCategory file_category() const noexcept = 0;
  [[nodiscard]] virtual FileMeta file_meta() const noexcept = 0;
  [[nodiscard]] virtual DecoderEngine decoder_engine() const noexcept = 0;

  [[nodiscard]] virtual bool password_encrypted() const noexcept {
    return false;
  }
  [[nodiscard]] virtual EncryptionState encryption_state() const noexcept {
    return EncryptionState::not_encrypted;
  }
  [[nodiscard]] virtual std::shared_ptr<DecodedFile>
  decrypt(const std::string &password) const {
    (void)password;
    return nullptr;
  }

  [[nodiscard]] virtual bool is_decodable() const noexcept = 0;
};

class TextFile : public DecodedFile {
public:
  [[nodiscard]] FileCategory file_category() const noexcept final {
    return FileCategory::text;
  }
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
  [[nodiscard]] virtual DocumentMeta document_meta() const = 0;

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
};

} // namespace odr::internal::abstract
