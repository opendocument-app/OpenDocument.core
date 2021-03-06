#ifndef ODR_INTERNAL_ABSTRACT_FILE_H
#define ODR_INTERNAL_ABSTRACT_FILE_H

#include <memory>

namespace odr {
enum class FileType;
enum class FileCategory;
enum class FileLocation;
struct FileMeta;
} // namespace odr

namespace odr::internal::abstract {
class Image;
class Archive;
class Document;

class File {
public:
  virtual ~File() = default;

  [[nodiscard]] virtual FileLocation location() const noexcept = 0;
  [[nodiscard]] virtual std::size_t size() const = 0;
  [[nodiscard]] virtual std::unique_ptr<std::istream> read() const = 0;
};

class DecodedFile {
public:
  virtual ~DecodedFile() = default;

  [[nodiscard]] virtual std::shared_ptr<File> file() const noexcept = 0;

  [[nodiscard]] virtual FileType file_type() const noexcept = 0;
  [[nodiscard]] virtual FileCategory file_category() const noexcept = 0;
  [[nodiscard]] virtual FileMeta file_meta() const noexcept = 0;
};

class ImageFile : public DecodedFile {
public:
  [[nodiscard]] FileCategory file_category() const noexcept final;

  [[nodiscard]] virtual std::shared_ptr<Image> image() const = 0;
};

class TextFile : public DecodedFile {
public:
  [[nodiscard]] FileCategory file_category() const noexcept final;
};

class ArchiveFile : public DecodedFile {
public:
  [[nodiscard]] FileCategory file_category() const noexcept final;

  [[nodiscard]] virtual std::shared_ptr<Archive> archive() const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_FILE_H
