#include <odr/internal/magic.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/open_strategy.hpp>
#include <odr/internal/util/string_util.hpp>

#include <istream>
#include <memory>
#include <string_view>
#include <vector>

namespace odr::internal {

namespace {

/// At most @p size bytes, cut back to what was actually read - a file shorter
/// than the longest signature must never be matched against what sat behind it.
std::string read_head(std::istream &in, const std::size_t size) {
  std::string result(size, '\0');
  in.read(result.data(), static_cast<std::streamsize>(size));
  result.resize(static_cast<std::size_t>(in.gcount()));
  return result;
}

bool match_magic(const std::string &head, const std::string &pattern) {
  const auto bytes = util::string::split(pattern, " ");
  if (bytes.size() > head.size()) {
    return false;
  }
  for (std::uint32_t i = 0; i < bytes.size(); ++i) {
    if (bytes[i] == "??") {
      continue;
    }
    if (const char num = static_cast<char>(std::stoul(bytes[i], nullptr, 16));
        head[i] != num) {
      return false;
    }
  }
  return true;
}

/// The four byte tag at @p offset, or empty if the head stops short of it.
std::string_view tag_at(const std::string &head, const std::size_t offset) {
  return head.size() < offset + 4 ? std::string_view{}
                                  : std::string_view(head).substr(offset, 4);
}

/// Which format a `RIFF` container holds, from its form tag.
FileType riff_file_type(const std::string &head) {
  const std::string_view form = tag_at(head, 8);
  if (form == "WEBP") {
    return FileType::webp;
  }
  if (form == "WAVE") {
    return FileType::waveform_audio;
  }
  if (form == "AVI ") {
    return FileType::audio_video_interleave;
  }
  return FileType::unknown;
}

/// Which format an ISO base media container holds, from its major brand.
///
/// The brands are open ended and every writer adds its own, so this names the
/// ones that mean something other than video and lets the rest - `isom`,
/// `mp41`, `mp42`, `avc1`, `dash` and whatever comes next - fall through to
/// what the container almost always is.
FileType iso_base_media_file_type(const std::string &head) {
  const std::string_view brand = tag_at(head, 8);
  if (brand == "qt  ") {
    return FileType::quicktime_video;
  }
  if (brand.starts_with("3g")) {
    return FileType::third_generation_partnership_video;
  }
  if (brand == "avif" || brand == "avis") {
    return FileType::av1_image_file_format;
  }
  if (brand.starts_with("hei") || brand.starts_with("hev") || brand == "mif1" ||
      brand == "msf1") {
    return FileType::high_efficiency_image_format;
  }
  if (brand.starts_with("M4A") || brand.starts_with("M4B")) {
    return FileType::mpeg4_audio;
  }
  return FileType::mpeg4_video;
}

} // namespace

FileType magic::file_type(const std::string &magic) {
  // https://en.wikipedia.org/wiki/List_of_file_signatures
  if (match_magic(magic, "50 4B 03 04")) {
    return FileType::zip;
  }
  if (match_magic(magic, "D0 CF 11 E0 A1 B1 1A E1")) {
    return FileType::compound_file_binary_format;
  }
  if (match_magic(magic, "25 50 44 46 2D")) {
    return FileType::portable_document_format;
  }
  if (match_magic(magic, "89 50 4E 47 0D 0A 1A 0A")) {
    return FileType::portable_network_graphics;
  }
  if (match_magic(magic, "FF D8 FF DB") ||
      match_magic(magic, "FF D8 FF E0 00 10 4A 46 49 46 00 01") ||
      match_magic(magic, "FF D8 FF EE") ||
      match_magic(magic, "FF D8 FF E1 ?? ?? 45 78 69 66 00 00")) {
    return FileType::jpeg;
  }
  if (match_magic(magic, "42 4D")) {
    return FileType::bitmap_image_file;
  }
  if (match_magic(magic, "47 49 46 38 37 61") ||
      match_magic(magic, "47 49 46 38 39 61")) {
    return FileType::graphics_interchange_format;
  }
  if (match_magic(magic, "56 43 4C 4D 54 46")) {
    return FileType::starview_metafile;
  }
  if (match_magic(magic, "7B 5C 72 74 66 31")) {
    return FileType::rich_text_format;
  }
  if (match_magic(magic, "FF 57 50 43")) {
    return FileType::word_perfect;
  }
  if (match_magic(magic, "4F 54 54 4F")) { // 'OTTO' — OpenType with CFF
    return FileType::opentype_font;
  }
  if (match_magic(magic, "00 01 00 00") || // TrueType outlines (sfnt 1.0)
      match_magic(magic, "74 72 75 65") || // 'true'
      match_magic(magic, "74 74 63 66")) { // 'ttcf' — TrueType Collection
    return FileType::truetype_font;
  }

  // the media formats below are named, not decoded: see `FileType`
  if (match_magic(magic, "52 49 46 46")) { // 'RIFF'
    if (const FileType file_type = riff_file_type(magic);
        file_type != FileType::unknown) {
      return file_type;
    }
  }
  if (match_magic(magic, "?? ?? ?? ?? 66 74 79 70")) { // 'ftyp' box
    return iso_base_media_file_type(magic);
  }
  if (match_magic(magic, "49 49 2A 00") || // little endian
      match_magic(magic, "4D 4D 00 2A")) { // big endian
    return FileType::tagged_image_file_format;
  }
  if (match_magic(magic, "1A 45 DF A3")) { // EBML
    return FileType::matroska_video;
  }
  if (match_magic(magic, "4F 67 67 53")) { // 'OggS'
    return FileType::ogg_audio;
  }
  if (match_magic(magic, "66 4C 61 43")) { // 'fLaC'
    return FileType::free_lossless_audio_codec;
  }
  if (match_magic(magic, "49 44 33") || // 'ID3' tag
      match_magic(magic, "FF FB") ||    // frame sync, no crc
      match_magic(magic, "FF F3") ||    //
      match_magic(magic, "FF F2") ||    //
      match_magic(magic, "FF FA")) {    // frame sync, with crc
    return FileType::mpeg_audio;
  }

  if (match_magic(magic, "00 00 01 00") || // icon
      match_magic(magic, "00 00 02 00")) { // cursor
    return FileType::windows_icon;
  }
  if (match_magic(magic, "00 00 00 0C 6A 50 20 20 0D 0A 87 0A") || // 'jP  ' box
      match_magic(magic, "FF 4F FF 51")) { // bare codestream
    return FileType::jpeg_2000;
  }
  if (match_magic(magic, "00 00 00 0C 4A 58 4C 20 0D 0A 87 0A")) { // 'JXL ' box
    return FileType::jpeg_xl;
  }
  if (match_magic(magic, "38 42 50 53")) { // '8BPS'
    return FileType::photoshop_document;
  }
  if (match_magic(magic, "D7 CD C6 9A") ||       // aldus placeable header
      match_magic(magic, "01 00 09 00 00 03") || // memory metafile header
      match_magic(magic, "02 00 09 00 00 03")) { // disk metafile header
    return FileType::windows_metafile;
  }
  // the leading record type alone is far too weak, so the header signature at
  // offset 40 has to agree
  if (match_magic(magic, "01 00 00 00") && tag_at(magic, 40) == " EMF") {
    return FileType::enhanced_metafile;
  }
  // two bytes and nothing more to check, so it goes after everything else
  if (match_magic(magic, "FF 0A")) { // bare codestream
    return FileType::jpeg_xl;
  }

  return FileType::unknown;
}

FileType magic::file_type(std::istream &in) {
  // every signature is a prefix but one: an enhanced metafile names itself at
  // offset 40, so the head has to reach 44
  static constexpr std::size_t head_size = 64;

  return file_type(read_head(in, head_size));
}

FileType magic::file_type(const abstract::File &file) {
  return file_type(*file.stream());
}

FileType magic::file_type(const File &file) {
  return file_type(*file.stream());
}

std::string_view magic::mimetype(const std::string &path,
                                 const Logger &logger) {
  // the signature table above is not enough on its own: a zip and a compound
  // file binary say nothing about which document they hold, and the answer for
  // those only comes out of opening them. that is what `list_file_types` does,
  // and it reports the container first and what was decoded from it after
  const std::vector<FileType> file_types =
      open_strategy::list_file_types(std::make_shared<DiskFile>(path), logger);
  if (file_types.empty()) {
    throw UnknownFileType();
  }

  return odr::mimetype_by_file_type(file_types.back());
}

} // namespace odr::internal
