#include <odr/internal/xml/xml_util.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/common/text_cursor.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <pugixml.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <string_view>
#include <tuple>

namespace odr::internal {

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
  if (const auto success = result.load_buffer(in.data(), in.size()); !success) {
    throw NoXmlFile();
  }
  return result;
}

pugi::xml_document xml::parse(std::istream &in) {
  return parse(util::stream::read(in));
}

void xml::set_attribute(pugi::xml_node node, const char *name,
                        const char *value) {
  pugi::xml_attribute attribute = node.attribute(name);
  if (!attribute) {
    attribute = node.append_attribute(name);
  }
  attribute.set_value(value);
}

pugi::xml_node
xml::insert_in_sequence(pugi::xml_node parent, const char *name,
                        const std::span<const std::string_view> order) {
  const auto rank = [&](const std::string_view child_name) {
    const auto it = std::ranges::find(order, child_name);
    return it == std::end(order)
               ? order.size()
               : static_cast<std::size_t>(it - std::begin(order));
  };
  const std::size_t own_rank = rank(name);
  for (const pugi::xml_node child : parent.children()) {
    if (rank(child.name()) > own_rank) {
      return parent.insert_child_before(name, child);
    }
  }
  return parent.append_child(name);
}

std::string xml::read_declared_encoding(std::istream &in) {
  const std::string probe = util::stream::read(in, 1024);
  std::string_view head(probe);
  if (head.starts_with("\xef\xbb\xbf")) {
    head.remove_prefix(3);
  }
  if (!head.starts_with("<?xml") || head.size() <= 5 ||
      !util::string::is_ascii_whitespace(head[5])) {
    return {};
  }
  const std::size_t end = head.find("?>");
  if (end == std::string_view::npos) {
    return {};
  }
  TextCursor cursor(head.substr(5, end - 5));
  while (!cursor.empty()) {
    cursor.skip_whitespace();
    const auto name = cursor.take_while(util::string::is_ascii_letter);
    if (name.empty() || !cursor.consume('=')) {
      return {};
    }
    cursor.skip_whitespace();
    const char quote = cursor.take();
    if (quote != '\'' && quote != '"') {
      return {};
    }
    const auto value = cursor.rest();
    const auto close = value.find(quote);
    if (close == std::string_view::npos) {
      return {};
    }
    if (name == "encoding") {
      return std::string(value.substr(0, close));
    }
    cursor.advance(close + 1);
  }
  return {};
}

/// Reads @p file once; pugixml's stream loader buffers it twice. The buffer is
/// `malloc`ed because pugixml takes it over and frees it, parse or no parse.
pugi::xml_document xml::parse(const abstract::File &file) {
  const std::size_t size = file.size();
  if (size == 0 || size > static_cast<std::size_t>(
                              std::numeric_limits<std::streamsize>::max())) {
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
  if (stream->bad() || stream->gcount() != static_cast<std::streamsize>(size)) {
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
  std::size_t token_start{0};
  auto close_token = [&](const std::size_t token_end,
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

  for (std::size_t i = 0; i < text.size(); ++i) {
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

} // namespace odr::internal
