#include <iostream>
#include <odr/internal/abstract/file.hpp>
#include <odr/internal/magic.hpp>
#include <odr/internal/util/string_util.hpp>

namespace odr::internal {

namespace {

bool match_magic(const std::string &head, const std::string &pattern) {
  auto bytes = internal::util::string::split(pattern, " ");
  if (bytes.size() > head.size()) {
    return false;
  }
  for (std::uint32_t i = 0; i < bytes.size(); ++i) {
    if (bytes[i] == "??") {
      continue;
    }
    auto num = (char)std::stoul(bytes[i], nullptr, 16);
    if (head[i] != num) {
      return false;
    }
  }
  return true;
}

} // namespace

FileType magic::file_type(const std::string &head) {
  // https://en.wikipedia.org/wiki/List_of_file_signatures
  if (match_magic(head, "50 4B 03 04")) {
    return FileType::zip;
  } else if (match_magic(head, "D0 CF 11 E0 A1 B1 1A E1")) {
    return FileType::compound_file_binary_format;
  } else if (match_magic(head, "25 50 44 46 2D")) {
    return FileType::portable_document_format;
  } else if (match_magic(head, "89 50 4E 47 0D 0A 1A 0A")) {
    return FileType::portable_network_graphics;
  } else if (match_magic(head, "FF D8 FF DB") ||
             match_magic(head, "FF D8 FF E0 00 10 4A 46 49 46 00 01") ||
             match_magic(head, "FF D8 FF EE") ||
             match_magic(head, "FF D8 FF E1 ?? ?? 45 78 69 66 00 00")) {
    return FileType::jpeg;
  } else if (match_magic(head, "42 4D")) {
    return FileType::bitmap_image_file;
  } else if (match_magic(head, "47 49 46 38 37 61") ||
             match_magic(head, "47 49 46 38 39 61")) {
    return FileType::graphics_interchange_format;
  } else if (match_magic(head, "56 43 4C 4D 54 46")) {
    return FileType::starview_metafile;
  } else if (match_magic(head, "7B 5C 72 74 66 31")) {
    return FileType::rich_text_format;
  } else if (match_magic(head, "FF 57 50 43")) {
    return FileType::word_perfect;
  }
  return FileType::unknown;
}

FileType magic::file_type(const internal::abstract::File &file) {
  static constexpr auto max_head_size = 12;

  char head[max_head_size];
  file.stream()->read(head, sizeof(head));

  return file_type(std::string(head, max_head_size));
}

} // namespace odr::internal
