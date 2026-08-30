#include <odr/internal/util/xml_util.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>

#include <pugixml.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <new>
#include <string_view>
#include <tuple>

namespace odr::internal::util {

// `PUGIXML_COMPACT` (CMakeLists.txt) changes the size of every node, and a
// translation unit that misses it links fine and then reads the wrong layout.
static_assert(sizeof(pugi::xml_node_struct) == 12);
static_assert(sizeof(pugi::xml_attribute_struct) == 8);

namespace {

/// Tab, line feed and carriage return are the only sub-`0x20` characters xml
/// 1.0 allows.
bool is_xml_control(const char c) {
  const auto value = static_cast<std::uint8_t>(c);
  return value < 0x20 && c != '\t' && c != '\n' && c != '\r';
}

std::string escape(const std::string_view text, const bool attribute) {
  std::string result;
  result.reserve(text.size());

  for (const char c : text) {
    switch (c) {
    case '&':
      result += "&amp;";
      break;
    case '<':
      result += "&lt;";
      break;
    case '>':
      result += "&gt;";
      break;
    case '"':
      result += attribute ? "&quot;" : "\"";
      break;
    default:
      if (!is_xml_control(c)) {
        result += c;
      }
      break;
    }
  }

  return result;
}

} // namespace

std::string xml::escape_text(const std::string_view text) {
  return escape(text, false);
}

std::string xml::escape_attribute(const std::string_view value) {
  return escape(value, true);
}

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

/// Reads @p file once; pugixml's stream loader buffers it twice. The buffer is
/// `malloc`ed because pugixml takes it over and frees it, parse or no parse.
pugi::xml_document xml::parse(const abstract::File &file) {
  const std::size_t size = file.size();
  if (size == 0) {
    throw NoXmlFile();
  }
  // before the buffer: opening an entry that is encrypted or compressed by a
  // method we do not have throws, and the size is the file's claim until then
  const std::unique_ptr<std::istream> stream = file.stream();

  std::unique_ptr<char, decltype(&std::free)> buffer(
      static_cast<char *>(std::malloc(size)), &std::free);
  if (buffer == nullptr) {
    throw std::bad_alloc();
  }

  stream->read(buffer.get(), static_cast<std::streamsize>(size));
  if (stream->gcount() != static_cast<std::streamsize>(size)) {
    throw NoXmlFile();
  }

  pugi::xml_document result;
  if (const auto success =
          result.load_buffer_inplace_own(buffer.release(), size);
      !success) {
    throw NoXmlFile();
  }
  return result;
}

pugi::xml_document xml::parse(const abstract::ReadableFilesystem &filesystem,
                              const AbsPath &path) {
  const std::shared_ptr<abstract::File> file = filesystem.open(path);
  if (!file) {
    throw FileNotFound();
  }
  return parse(*file);
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
