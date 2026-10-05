#include <odr/internal/rtf/rtf_tokenizer.hpp>

#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace odr::internal::rtf {

namespace str = util::string;

namespace {

std::uint8_t hex_char_to_int(const char c) {
  const std::optional<std::uint8_t> value = str::hex_digit(c);
  if (!value) {
    throw std::runtime_error("invalid hex digit");
  }
  return *value;
}

char two_hex_to_char(const char first, const char second) {
  return static_cast<char>((hex_char_to_int(first) << 4) |
                           hex_char_to_int(second));
}

} // namespace

Tokenizer::Tokenizer(std::istream &in) : m_in{&in}, m_sb{in.rdbuf()} {
  const std::istream::sentry sentry(in, true);
  if (!sentry) {
    throw std::ios_base::failure("rtf: unreadable input stream");
  }
}

Tokenizer::int_type Tokenizer::geti() {
  const int_type c = m_sb->sgetc();
  if (c == eof) {
    m_in->setstate(std::ios::eofbit);
  }
  return c;
}

Tokenizer::char_type Tokenizer::bumpc() {
  const int_type c = m_sb->sbumpc();
  if (c == eof) {
    m_in->setstate(std::ios::eofbit);
    throw std::runtime_error("unexpected stream exhaust");
  }
  return static_cast<char_type>(c);
}

Token Tokenizer::read_token() {
  const int_type i = geti();
  if (i == eof) {
    return End{};
  }
  switch (static_cast<char_type>(i)) {
  case '{':
    bumpc();
    return GroupOpen{};
  case '}':
    bumpc();
    return GroupClose{};
  case '\\':
    bumpc();
    return read_control();
  default:
    return read_text();
  }
}

Token Tokenizer::read_control() {
  const int_type i = geti();
  if (i == eof) {
    throw std::runtime_error("rtf: trailing backslash");
  }

  if (const auto c = static_cast<char_type>(i); !str::is_ascii_letter(c)) {
    bumpc();
    if (c == '\'') {
      const char_type first = bumpc();
      const char_type second = bumpc();
      return HexEscape{two_hex_to_char(first, second)};
    }
    // a control symbol takes no delimiter: a space after it is text
    return ControlSymbol{c};
  }

  std::string name;
  while (true) {
    const int_type letter = geti();
    if (letter == eof ||
        !str::is_ascii_letter(static_cast<char_type>(letter))) {
      break;
    }
    name.push_back(bumpc());
  }

  // The parameter's terminator is a delimiter under the same rule as the
  // name's: a space is consumed, anything else stays unread. `\fs24 Text`
  // therefore starts its text at `T`, and `\bin4 ` puts the payload right
  // after the consumed space.
  std::optional<std::int32_t> parameter;
  if (const int_type delimiter = geti(); delimiter != eof) {
    const auto d = static_cast<char_type>(delimiter);
    if (d == '-' || str::is_ascii_digit(d)) {
      const bool negative = d == '-';
      if (negative) {
        bumpc();
      }
      std::int64_t value = 0;
      std::size_t digits = 0;
      while (digits < max_parameter_digits) {
        const int_type digit = geti();
        if (digit == eof ||
            !str::is_ascii_digit(static_cast<char_type>(digit))) {
          break;
        }
        value = value * 10 + (bumpc() - '0');
        ++digits;
      }
      if (digits > 0) {
        if (negative) {
          value = -value;
        }
        value = std::min<std::int64_t>(
            std::max<std::int64_t>(value,
                                   std::numeric_limits<std::int32_t>::min()),
            std::numeric_limits<std::int32_t>::max());
        parameter = static_cast<std::int32_t>(value);
      }
      if (geti() == ' ') {
        bumpc();
      }
    } else if (d == ' ') {
      bumpc();
    }
  }

  // `\binN` is the one control word whose payload the tokenizer has to read
  // itself: the bytes are raw, so a brace-counting scan over them would
  // desync the group nesting.
  if (name == "bin") {
    const std::int32_t n = parameter.value_or(0);
    return Binary{n > 0 ? util::byte_stream::read_u8s(
                              *m_in, static_cast<std::uint32_t>(n))
                        : std::string()};
  }

  return ControlWord{std::move(name), parameter};
}

Token Tokenizer::read_text() {
  std::string bytes;
  while (true) {
    const int_type i = geti();
    if (i == eof) {
      break;
    }
    const auto c = static_cast<char_type>(i);
    if (c == '{' || c == '}' || c == '\\') {
      break;
    }
    bumpc();
    // a bare line break is not text (*Conventions of an RTF Reader*)
    if (c != '\r' && c != '\n') {
      bytes.push_back(c);
    }
  }
  return Text{std::move(bytes)};
}

} // namespace odr::internal::rtf
