#include <internal/util/xml_util.h>

#include <odr/exceptions.h>

#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>

#include <pugixml.hpp>

#include <cstdint>
#include <memory>
#include <utility>

namespace odr::internal::util {

pugi::xml_document xml::parse(const std::string &in) {
  pugi::xml_document result;
  const auto success = result.load_string(in.c_str());
  if (!success) {
    throw NoXml();
  }
  return result;
}

pugi::xml_document xml::parse(std::istream &in) {
  pugi::xml_document result;
  const auto success = result.load(in);
  if (!success) {
    throw NoXml();
  }
  return result;
}

pugi::xml_document xml::parse(const abstract::ReadableFilesystem &filesystem,
                              const common::Path &path) {
  pugi::xml_document result;
  auto file = filesystem.open(path);
  if (!file) {
    throw FileNotFound();
  }
  const auto success = result.load(*file->stream());
  if (!success) {
    throw NoXml();
  }
  return result;
}

xml::StringToken::StringToken(const Type type, std::string string)
    : type{type}, string{std::move(string)} {}

std::vector<xml::StringToken> xml::tokenize_text(const std::string &text) {
  std::vector<xml::StringToken> result;

  StringToken::Type token_type{StringToken::Type::none};
  std::uint32_t token_start{0};
  auto close_token = [&](const std::uint32_t token_end,
                         const StringToken::Type new_token_type) {
    if (token_type == new_token_type) {
      return;
    }
    if (token_end > token_start) {
      result.emplace_back(token_type,
                          text.substr(token_start, token_end - token_start));
    }
    token_start = token_end;
    token_type = new_token_type;
  };

  for (std::uint32_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\t') {
      close_token(i, StringToken::Type::tabs);
    } else if ((text[i] == ' ') &&
               (((i > 0) && (text[i - 1] == ' ')) ||
                ((i + 1 < text.size()) && (text[i + 1] == ' ')))) {
      close_token(i, StringToken::Type::spaces);
    } else {
      close_token(i, StringToken::Type::string);
    }
  }
  close_token(text.size(), StringToken::Type::none);

  return result;
}

} // namespace odr::internal::util
