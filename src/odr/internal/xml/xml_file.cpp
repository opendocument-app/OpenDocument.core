#include <odr/internal/xml/xml_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/odr.hpp>

#include <odr/internal/encoding/transcode.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/xml_util.hpp>

#include <pugixml.hpp>

#include <istream>
#include <tuple>
#include <utility>

namespace odr::internal {

namespace {

/// The declaration beats any guess over the bytes; a name we do not know
/// leaves the guess in place.
TextEncoding resolve_encoding(const text::TextFile &file) {
  const std::unique_ptr<std::istream> in = file.file()->stream();
  const std::string declared = util::xml::read_declared_encoding(*in);

  if (const TextEncoding encoding = text_encoding_by_name(declared);
      encoding != TextEncoding::unknown) {
    return encoding;
  }
  return file.encoding();
}

} // namespace

pugi::xml_document xml::parse_source(const std::string &text) {
  // `parse_full` adds the four node kinds `parse_default` drops, all of which
  // a viewer has to show; `parse_ws_pcdata_single` keeps `<a>   </a>` while
  // dropping the newline between two siblings.
  static constexpr unsigned int options =
      pugi::parse_full | pugi::parse_ws_pcdata_single;

  pugi::xml_document result;
  if (const pugi::xml_parse_result success = result.load_buffer(
          text.data(), text.size(), options, pugi::encoding_utf8);
      !success) {
    throw NoXmlFile();
  }
  return result;
}

xml::XmlFile::XmlFile(std::shared_ptr<text::TextFile> file)
    : m_file{std::move(file)} {
  m_encoding = resolve_encoding(*m_file);
  std::ignore = parse_source(text());
}

std::shared_ptr<abstract::File> xml::XmlFile::file() const noexcept {
  return m_file->file();
}

FileType xml::XmlFile::file_type() const noexcept { return FileType::xml; }

std::string_view xml::XmlFile::mimetype() const noexcept {
  return "application/xml";
}

FileMeta xml::XmlFile::file_meta() const noexcept {
  FileMeta result;
  result.type = file_type();
  result.mimetype = mimetype();
  return result;
}

bool xml::XmlFile::is_decodable() const noexcept { return false; }

TextEncoding xml::XmlFile::encoding() const noexcept { return m_encoding; }

std::string xml::XmlFile::text() const {
  // a parser only ever sees utf-8; there is no handing the bytes on undecoded
  if (!text_encoding_is_decodable(m_encoding)) {
    throw UnsupportedTextEncoding(m_encoding);
  }
  const std::unique_ptr<std::istream> in = m_file->file()->stream();
  return encoding::to_utf8(util::stream::read(*in), m_encoding);
}

} // namespace odr::internal
