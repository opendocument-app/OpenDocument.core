#pragma once

#include <odr/file.hpp>

#include <odr/internal/xml/xml_file.hpp>

#include <memory>
#include <string>

namespace odr::internal::svg {

/// An image whose bytes are an xml document, so the xml decoder is what
/// recognises it and what resolves its encoding.
class SvgFile final : public abstract::ImageFile {
public:
  /// @throws NoSvgFile if @p file's root element is not `svg`.
  explicit SvgFile(std::shared_ptr<xml::XmlFile> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  [[nodiscard]] std::shared_ptr<abstract::Image> image() const override;

  /// The document's markup, decoded to utf-8.
  [[nodiscard]] std::string text() const;

private:
  std::shared_ptr<xml::XmlFile> m_file;
};

} // namespace odr::internal::svg
