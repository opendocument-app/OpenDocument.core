#include <odr/internal/magic.hpp>

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/util/string_util.hpp>

#include <iostream>

namespace odr::internal {

namespace {

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
  return FileType::unknown;
}

FileType magic::file_type(std::istream &in) {
  static constexpr auto max_head_size = 12;

  char head[max_head_size];
  in.read(head, sizeof(head));

  return file_type(std::string(head, max_head_size));
}

FileType magic::file_type(const abstract::File &file) {
  return file_type(*file.stream());
}

FileType magic::file_type(const File &file) {
  return file_type(*file.stream());
}

} // namespace odr::internal
