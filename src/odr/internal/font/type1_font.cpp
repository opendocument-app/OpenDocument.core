#include <odr/internal/font/type1_font.hpp>

#include <odr/internal/font/type1_crypt.hpp>
#include <odr/internal/util/byte_util.hpp>

#include <charconv>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::font::type1 {

namespace {

[[nodiscard]] bool is_ps_space(const char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' ||
         c == '\0';
}

/// Skip PostScript whitespace starting at @p p.
[[nodiscard]] std::size_t skip_space(const std::string_view s, std::size_t p) {
  while (p < s.size() && is_ps_space(s[p])) {
    ++p;
  }
  return p;
}

/// Read a whitespace-delimited token starting at @p p; advances @p p past it.
[[nodiscard]] std::string_view read_token(const std::string_view s,
                                          std::size_t &p) {
  p = skip_space(s, p);
  const std::size_t begin = p;
  while (p < s.size() && !is_ps_space(s[p])) {
    ++p;
  }
  return s.substr(begin, p - begin);
}

[[nodiscard]] bool parse_int(const std::string_view token, std::int32_t &out) {
  const char *begin = token.data();
  const char *end = begin + token.size();
  const auto [ptr, ec] = std::from_chars(begin, end, out);
  return ec == std::errc() && ptr == end;
}

[[nodiscard]] double parse_double(const std::string_view token) {
  // std::from_chars for double is not universally available; std::stod needs a
  // null-terminated copy.
  try {
    return std::stod(std::string(token));
  } catch (const std::exception &) {
    return 0.0;
  }
}

/// Parse the numbers inside the next `[...]` or `{...}` after @p key in @p s.
[[nodiscard]] std::vector<double>
parse_number_array(const std::string_view s, const std::string_view key) {
  std::vector<double> out;
  const std::size_t k = s.find(key);
  if (k == std::string_view::npos) {
    return out;
  }
  std::size_t open = s.find_first_of("[{", k);
  if (open == std::string_view::npos) {
    return out;
  }
  const std::size_t close = s.find_first_of("]}", open);
  if (close == std::string_view::npos) {
    return out;
  }
  std::size_t p = open + 1;
  while (p < close) {
    const std::string_view token = read_token(s.substr(0, close), p);
    if (token.empty()) {
      break;
    }
    out.push_back(parse_double(token));
  }
  return out;
}

/// Read the `length` binary bytes of an `RD`/`-|` value: at @p p sits the
/// length integer, then the RD operator, then exactly one space, then the
/// bytes. On success returns the bytes and advances @p p past them; on failure
/// returns nullopt.
[[nodiscard]] std::optional<std::string_view>
read_rd_binary(const std::string_view s, std::size_t &p) {
  std::size_t q = p;
  const std::string_view length_token = read_token(s, q);
  std::int32_t length = 0;
  if (!parse_int(length_token, length) || length < 0) {
    return std::nullopt;
  }
  const std::string_view rd = read_token(s, q); // "RD" or "-|"
  if (rd != "RD" && rd != "-|") {
    return std::nullopt;
  }
  // Exactly one space separates the RD operator from the binary data.
  if (q >= s.size()) {
    return std::nullopt;
  }
  ++q; // the single delimiter space
  if (q + static_cast<std::size_t>(length) > s.size()) {
    return std::nullopt;
  }
  const std::string_view bytes = s.substr(q, static_cast<std::size_t>(length));
  p = q + static_cast<std::size_t>(length);
  return bytes;
}

} // namespace

bool Type1Font::is_type1(const std::string_view data) {
  if (data.size() >= 2 && static_cast<std::uint8_t>(data[0]) == 0x80) {
    return true; // PFB segment marker
  }
  return data.substr(0, 256).find("%!PS-AdobeFont") != std::string_view::npos ||
         data.substr(0, 256).find("%!FontType1") != std::string_view::npos;
}

Type1Font::Type1Font(std::string_view data) {
  // Strip PFB segment framing if present: each segment is `0x80 type len32le`
  // followed by `len` bytes (type 1 = ASCII, 2 = binary, 3 = EOF).
  std::string unframed;
  if (!data.empty() && static_cast<std::uint8_t>(data[0]) == 0x80) {
    std::size_t p = 0;
    while (p + 6 <= data.size() && static_cast<std::uint8_t>(data[p]) == 0x80) {
      const std::uint8_t type = static_cast<std::uint8_t>(data[p + 1]);
      if (type == 3) {
        break;
      }
      const auto len =
          util::byte::from_little_endian<std::uint32_t>(data.substr(p + 2, 4));
      p += 6;
      if (p + len > data.size()) {
        break;
      }
      unframed.append(data.substr(p, len));
      p += len;
    }
    data = unframed;
  }

  const std::size_t eexec = data.find("eexec");
  if (eexec == std::string_view::npos) {
    throw std::runtime_error("type1: no eexec section");
  }

  parse_clear(data.substr(0, eexec));

  // The encrypted blob begins after `eexec` and its trailing whitespace.
  const std::size_t blob = skip_space(data, eexec + 5);
  const std::string decrypted = decrypt_eexec(data.substr(blob));
  parse_private(decrypted);

  if (m_glyphs.empty()) {
    throw std::runtime_error("type1: no /CharStrings");
  }
}

void Type1Font::parse_clear(const std::string_view clear) {
  if (const std::size_t k = clear.find("/FontName");
      k != std::string_view::npos) {
    std::size_t p = clear.find('/', k + 1);
    if (p != std::string_view::npos) {
      ++p;
      m_name = std::string(read_token(clear, p));
    }
  }

  if (const std::vector<double> matrix =
          parse_number_array(clear, "/FontMatrix");
      matrix.size() == 6) {
    m_font_matrix = {matrix[0], matrix[1], matrix[2],
                     matrix[3], matrix[4], matrix[5]};
  }
  if (const std::vector<double> bbox = parse_number_array(clear, "/FontBBox");
      bbox.size() == 4) {
    m_font_bbox = {
        static_cast<std::int16_t>(bbox[0]), static_cast<std::int16_t>(bbox[1]),
        static_cast<std::int16_t>(bbox[2]), static_cast<std::int16_t>(bbox[3])};
  }

  // /Encoding: `StandardEncoding def`, or a custom array built with
  // `dup <code> /<name> put` lines.
  const std::size_t enc = clear.find("/Encoding");
  if (enc != std::string_view::npos) {
    const std::string_view after = clear.substr(enc);
    if (after.substr(0, 64).find("StandardEncoding") !=
        std::string_view::npos) {
      m_standard_encoding = true;
    } else {
      m_standard_encoding = false;
      std::size_t p = 0;
      while ((p = after.find("dup ", p)) != std::string_view::npos) {
        std::size_t q = p + 4;
        std::int32_t code = 0;
        const std::string_view code_token = read_token(after, q);
        const std::size_t slash = after.find('/', q);
        if (parse_int(code_token, code) && slash != std::string_view::npos) {
          std::size_t r = slash + 1;
          m_encoding[code] = std::string(read_token(after, r));
        }
        p = q;
      }
    }
  }
}

void Type1Font::parse_private(const std::string_view decrypted) {
  std::int32_t len_iv = 4;
  if (const std::size_t k = decrypted.find("/lenIV");
      k != std::string_view::npos) {
    std::size_t p = k + 6;
    std::int32_t value = 0;
    if (parse_int(read_token(decrypted, p), value)) {
      len_iv = value;
    }
  }
  m_len_iv = len_iv;

  // /Subrs: entries `dup <index> <length> RD <bytes> NP`.
  if (const std::size_t k = decrypted.find("/Subrs");
      k != std::string_view::npos) {
    std::size_t p = k;
    while ((p = decrypted.find("dup ", p)) != std::string_view::npos) {
      // Stop when /CharStrings starts (Subrs precede it).
      const std::size_t cs = decrypted.find("/CharStrings");
      if (cs != std::string_view::npos && p > cs) {
        break;
      }
      std::size_t q = p + 4;
      std::int32_t index = 0;
      if (!parse_int(read_token(decrypted, q), index) || index < 0) {
        p += 4;
        continue;
      }
      const std::optional<std::string_view> bytes =
          read_rd_binary(decrypted, q);
      if (!bytes.has_value()) {
        p += 4;
        continue;
      }
      if (static_cast<std::int32_t>(m_subrs.size()) <= index) {
        m_subrs.resize(index + 1);
      }
      m_subrs[index] = decrypt_charstring(*bytes, len_iv);
      p = q;
    }
  }

  // /CharStrings: entries `/<name> <length> RD <bytes> ND`.
  const std::size_t cs = decrypted.find("/CharStrings");
  if (cs == std::string_view::npos) {
    return;
  }
  const std::size_t begin = decrypted.find("begin", cs);
  std::size_t p = (begin == std::string_view::npos) ? cs : begin + 5;
  while (p < decrypted.size()) {
    const std::size_t slash = decrypted.find('/', p);
    if (slash == std::string_view::npos) {
      break;
    }
    std::size_t q = slash + 1;
    std::string name(read_token(decrypted, q));
    const std::optional<std::string_view> bytes = read_rd_binary(decrypted, q);
    if (!bytes.has_value()) {
      // Not a charstring entry (e.g. `end`); advance past this slash.
      p = slash + 1;
      continue;
    }
    m_glyphs.push_back({std::move(name), decrypt_charstring(*bytes, len_iv)});
    p = q;
  }
}

} // namespace odr::internal::font::type1
