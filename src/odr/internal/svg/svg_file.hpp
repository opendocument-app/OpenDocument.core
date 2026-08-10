#pragma once

#include <odr/file.hpp>

#include <odr/internal/xml/xml_file.hpp>

#include <memory>
#include <string>

namespace odr::internal::svg {

/// Whether @p file's root element is `svg`, which is all that tells an svg from
/// any other xml - and what @ref SvgFile's constructor throws on.
[[nodiscard]] bool is_svg_file(const xml::XmlFile &file);

/// An image whose bytes are an xml document, so the xml decoder is what
/// recognises it and what resolves its encoding. Nothing is drawn from the
/// tree - the markup goes into a page as the `<img>` data url every other
/// image does.
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

  /// The xml file underneath, and the text file under that: what parsed the
  /// markup, and what holds the bytes.
  [[nodiscard]] std::shared_ptr<xml::XmlFile> xml_file() const noexcept;
  [[nodiscard]] std::shared_ptr<text::TextFile> text_file() const noexcept;

  /// @ref xml_file's tree.
  [[nodiscard]] const pugi::xml_document &document() const noexcept;

  /// The document's markup, decoded to utf-8.
  /// @throws UnsupportedTextEncoding if its encoding cannot be decoded.
  [[nodiscard]] std::string text() const;

private:
  std::shared_ptr<xml::XmlFile> m_file;
};

} // namespace odr::internal::svg
