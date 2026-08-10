#include <odr/internal/util/xml_util.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>

#include <pugixml.hpp>

#include <cstddef>
#include <string_view>
#include <tuple>

namespace odr::internal::util {

pugi::xml_document xml::parse(const std::string &in) {
  pugi::xml_document result;
  if (const auto success = result.load_string(in.c_str()); !success) {
    throw NoXmlFile();
  }
  return result;
}

pugi::xml_document xml::parse(std::istream &in) {
  pugi::xml_document result;
  if (const auto success = result.load(in); !success) {
    throw NoXmlFile();
  }
  return result;
}

void xml::check_xml_file(std::istream &in) { std::ignore = parse(in); }

std::string xml::read_declared_encoding(std::istream &in) {
  static constexpr std::size_t probe_size = 1024;
  static constexpr std::string_view space = " \t\r\n";

  std::string probe(probe_size, '\0');
  in.read(probe.data(), static_cast<std::streamsize>(probe.size()));
  probe.resize(static_cast<std::size_t>(in.gcount()));

  std::string_view head(probe);
  if (head.starts_with("\xef\xbb\xbf")) {
    head.remove_prefix(3);
  }
  if (!head.starts_with("<?xml")) {
    return {};
  }
  const std::size_t declaration_end = head.find("?>");
  if (declaration_end == std::string_view::npos) {
    return {};
  }
  head = head.substr(0, declaration_end);

  const std::size_t name = head.find("encoding");
  if (name == std::string_view::npos) {
    return {};
  }
  head.remove_prefix(name + std::string_view("encoding").size());

  std::size_t at = head.find_first_not_of(space);
  if (at == std::string_view::npos || head[at] != '=') {
    return {};
  }
  at = head.find_first_not_of(space, at + 1);
  if (at == std::string_view::npos || (head[at] != '"' && head[at] != '\'')) {
    return {};
  }
  const char quote = head[at];
  ++at;
  const std::size_t value_end = head.find(quote, at);
  if (value_end == std::string_view::npos) {
    return {};
  }
  return std::string(head.substr(at, value_end - at));
}

pugi::xml_document xml::parse(const abstract::ReadableFilesystem &filesystem,
                              const AbsPath &path) {
  pugi::xml_document result;
  auto file = filesystem.open(path);
  if (!file) {
    throw FileNotFound();
  }
  if (const auto success = result.load(*file->stream()); !success) {
    throw NoXmlFile();
  }
  return result;
}

xml::StringToken::StringToken(const Type type, std::string string)
    : type{type}, string{std::move(string)} {}

std::vector<xml::StringToken> xml::tokenize_text(const std::string &text) {
  std::vector<StringToken> result;

  auto token_type{StringToken::Type::none};
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
    } else if (text[i] == ' ' &&
               ((i > 0 && text[i - 1] == ' ') ||
                (i + 1 < text.size() && text[i + 1] == ' '))) {
      close_token(i, StringToken::Type::spaces);
    } else {
      close_token(i, StringToken::Type::string);
    }
  }
  close_token(text.size(), StringToken::Type::none);

  return result;
}

} // namespace odr::internal::util
