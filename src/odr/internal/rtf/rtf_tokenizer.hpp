#pragma once

#include <odr/internal/rtf/rtf_token.hpp>

#include <cstddef>
#include <istream>
#include <string>

namespace odr::internal::rtf {

/// Pull-based token stream over the rtf byte grammar (*Conventions of an RTF
/// Reader*). Groups, destinations and encodings are the parser's business;
/// this only splits bytes into tokens.
class Tokenizer final {
public:
  using char_type = std::streambuf::char_type;
  using int_type = std::streambuf::int_type;
  static constexpr int_type eof = std::streambuf::traits_type::eof();

  explicit Tokenizer(std::istream &in);

  /// The next token; `End` once the stream is exhausted.
  [[nodiscard]] Token read_token();

private:
  /// *Control Word*: at most 10 digits, which also bounds the value the clamp
  /// has to fit into `std::int32_t`.
  static constexpr std::size_t max_parameter_digits = 10;

  int_type geti();
  char_type bumpc();
  std::string bumpnc(std::size_t n);

  /// With the leading `\` already consumed.
  [[nodiscard]] Token read_control();
  [[nodiscard]] Token read_text();

  std::istream *m_in{nullptr};
  std::streambuf *m_sb{nullptr};
};

} // namespace odr::internal::rtf
