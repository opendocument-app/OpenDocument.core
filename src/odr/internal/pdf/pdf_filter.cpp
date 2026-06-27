#include <odr/internal/pdf/pdf_filter.hpp>

#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace odr::internal::pdf {
namespace {

/// Inline-image abbreviations are accepted for regular streams, too; real
/// producers use them there.
std::string canonical_filter_name(const std::string &name) {
  if (name == "AHx") {
    return "ASCIIHexDecode";
  }
  if (name == "A85") {
    return "ASCII85Decode";
  }
  if (name == "LZW") {
    return "LZWDecode";
  }
  if (name == "Fl") {
    return "FlateDecode";
  }
  if (name == "RL") {
    return "RunLengthDecode";
  }
  if (name == "CCF") {
    return "CCITTFaxDecode";
  }
  if (name == "DCT") {
    return "DCTDecode";
  }
  return name;
}

bool is_image_codec(const std::string &name) {
  return name == "DCTDecode" || name == "JPXDecode" ||
         name == "CCITTFaxDecode" || name == "JBIG2Decode";
}

Integer parms_integer(const Object &parms, const std::string &key,
                      const Integer default_value) {
  if (!parms.is_dictionary()) {
    return default_value;
  }
  const Dictionary &dictionary = parms.as_dictionary();
  const Object &value = dictionary.get(key);
  return value.is_integer() ? value.as_integer() : default_value;
}

std::string apply_filter(const std::string &name, const Object &parms,
                         std::string data) {
  if (name == "FlateDecode" || name == "LZWDecode") {
    if (name == "FlateDecode") {
      data = crypto::util::zlib_inflate(data);
    } else {
      data = lzw_decode(data, parms_integer(parms, "EarlyChange", 1));
    }
    if (const Integer predictor = parms_integer(parms, "Predictor", 1);
        predictor > 1) {
      data = apply_predictor(std::move(data), predictor,
                             parms_integer(parms, "Colors", 1),
                             parms_integer(parms, "BitsPerComponent", 8),
                             parms_integer(parms, "Columns", 1));
    }
    return data;
  }
  if (name == "ASCIIHexDecode") {
    return ascii_hex_decode(data);
  }
  if (name == "ASCII85Decode") {
    return ascii85_decode(data);
  }
  if (name == "RunLengthDecode") {
    return run_length_decode(data);
  }
  if (name == "Crypt") {
    // Only the Identity crypt filter passes through here (7.4.10); actual
    // decryption is the security handler's job.
    if (!parms.is_dictionary() || !parms.as_dictionary().has_key("Name") ||
        parms.as_dictionary()["Name"].as_string() == "Identity") {
      return data;
    }
    throw std::runtime_error("unsupported Crypt filter: " +
                             parms.as_dictionary()["Name"].as_string());
  }
  throw std::runtime_error("unknown stream filter: " + name);
}

std::string apply_tiff_predictor(std::string data, const Integer colors,
                                 const Integer bits_per_component,
                                 const Integer columns) {
  if (bits_per_component != 8 && bits_per_component != 16) {
    throw std::runtime_error("unsupported TIFF predictor bits per component: " +
                             std::to_string(bits_per_component));
  }

  const std::size_t component_bytes = bits_per_component / 8;
  const std::size_t pixel_bytes = colors * component_bytes;
  const std::size_t row_bytes = columns * pixel_bytes;
  if (row_bytes == 0 || data.size() % row_bytes != 0) {
    throw std::runtime_error("TIFF predictor: data not a whole number of rows");
  }

  for (std::size_t row = 0; row < data.size(); row += row_bytes) {
    for (std::size_t i = pixel_bytes; i < row_bytes; i += component_bytes) {
      if (component_bytes == 1) {
        data[row + i] = static_cast<char>(
            static_cast<std::uint8_t>(data[row + i]) +
            static_cast<std::uint8_t>(data[row + i - pixel_bytes]));
      } else {
        const std::uint32_t left =
            (static_cast<std::uint8_t>(data[row + i - pixel_bytes]) << 8) |
            static_cast<std::uint8_t>(data[row + i - pixel_bytes + 1]);
        const std::uint32_t value =
            (static_cast<std::uint8_t>(data[row + i]) << 8) |
            static_cast<std::uint8_t>(data[row + i + 1]);
        const std::uint32_t sum = value + left;
        data[row + i] = static_cast<char>((sum >> 8) & 0xff);
        data[row + i + 1] = static_cast<char>(sum & 0xff);
      }
    }
  }

  return data;
}

std::string apply_png_predictor(const std::string &data, const Integer colors,
                                const Integer bits_per_component,
                                const Integer columns) {
  const std::size_t row_bytes = (columns * colors * bits_per_component + 7) / 8;
  const std::size_t pixel_bytes =
      std::max<Integer>(1, colors * bits_per_component / 8);
  if (row_bytes == 0 || data.size() % (row_bytes + 1) != 0) {
    throw std::runtime_error("PNG predictor: data not a whole number of rows");
  }

  std::string result;
  result.reserve(data.size());
  std::string up_row(row_bytes, '\0');
  std::string row(row_bytes, '\0');

  for (std::size_t in = 0; in < data.size(); in += row_bytes + 1) {
    const auto tag = static_cast<std::uint8_t>(data[in]);
    for (std::size_t i = 0; i < row_bytes; ++i) {
      const auto raw = static_cast<std::uint8_t>(data[in + 1 + i]);
      const std::uint8_t left =
          i >= pixel_bytes ? static_cast<std::uint8_t>(row[i - pixel_bytes])
                           : 0;
      const auto up = static_cast<std::uint8_t>(up_row[i]);
      const std::uint8_t up_left =
          i >= pixel_bytes ? static_cast<std::uint8_t>(up_row[i - pixel_bytes])
                           : 0;

      std::uint8_t value;
      switch (tag) {
      case 0: // None
        value = raw;
        break;
      case 1: // Sub
        value = raw + left;
        break;
      case 2: // Up
        value = raw + up;
        break;
      case 3: // Average
        value = raw + static_cast<std::uint8_t>((left + up) / 2);
        break;
      case 4: { // Paeth
        const std::int32_t p = left + up - up_left;
        const std::int32_t pa = std::abs(p - left);
        const std::int32_t pb = std::abs(p - up);
        const std::int32_t pc = std::abs(p - up_left);
        std::uint8_t predicted;
        if (pa <= pb && pa <= pc) {
          predicted = left;
        } else if (pb <= pc) {
          predicted = up;
        } else {
          predicted = up_left;
        }
        value = raw + predicted;
        break;
      }
      default:
        throw std::runtime_error("PNG predictor: unknown row tag " +
                                 std::to_string(tag));
      }
      row[i] = static_cast<char>(value);
    }
    result += row;
    std::swap(up_row, row);
  }

  return result;
}

} // namespace
} // namespace odr::internal::pdf

namespace odr::internal {

pdf::DecodeResult pdf::decode(const Object &filter, const Object &decode_parms,
                              std::string data) {
  DecodeResult result;

  std::vector<Object> filters;
  if (filter.is_array()) {
    for (const Object &entry : filter.as_array()) {
      filters.push_back(entry);
    }
  } else if (!filter.is_null()) {
    filters.push_back(filter);
  }

  const auto parms_for = [&](const std::size_t i) -> Object {
    if (decode_parms.is_array()) {
      const Array &array = decode_parms.as_array();
      return i < array.size() ? array[i] : Object();
    }
    return i == 0 ? decode_parms : Object();
  };

  for (std::size_t i = 0; i < filters.size(); ++i) {
    const std::string name = canonical_filter_name(filters[i].as_string());
    const Object parms = parms_for(i);
    if (is_image_codec(name)) {
      result.stopped_at_filter = name;
      result.stopped_at_parms = parms;
      break;
    }
    data = apply_filter(name, parms, std::move(data));
  }

  result.data = std::move(data);
  return result;
}

std::optional<std::string> pdf::terminal_image_codec(const Object &filter) {
  Object last;
  if (filter.is_array()) {
    const Array &array = filter.as_array();
    if (array.empty()) {
      return std::nullopt;
    }
    last = array.back();
  } else if (!filter.is_null()) {
    last = filter;
  } else {
    return std::nullopt;
  }

  std::string name = canonical_filter_name(last.as_string());
  if (is_image_codec(name)) {
    return name;
  }
  return std::nullopt;
}

std::string pdf::ascii_hex_decode(const std::string &input) {
  std::string result;
  result.reserve(input.size() / 2);

  std::optional<std::uint8_t> first;
  for (const char c : input) {
    if (ObjectParser::is_whitespace(c)) {
      continue;
    }
    if (c == '>') {
      break;
    }
    const std::uint8_t value = ObjectParser::hex_char_to_int(c);
    if (first.has_value()) {
      result.push_back(static_cast<char>((*first << 4) | value));
      first.reset();
    } else {
      first = value;
    }
  }
  if (first.has_value()) {
    // odd number of digits: behave as if a 0 followed (7.4.2)
    result.push_back(static_cast<char>(*first << 4));
  }

  return result;
}

std::string pdf::ascii85_decode(const std::string &input) {
  std::string result;
  result.reserve(input.size() * 4 / 5);

  std::array<std::uint32_t, 5> digits{};
  std::size_t n = 0;

  const auto flush = [&](const std::size_t count) {
    for (std::size_t i = count; i < 5; ++i) {
      digits[i] = 84; // pad as if 'u'
    }
    std::uint64_t value = 0;
    for (const std::uint32_t digit : digits) {
      value = value * 85 + digit;
    }
    if (value > 0xffffffffu) {
      throw std::runtime_error("ASCII85Decode group out of range");
    }
    for (std::size_t i = 0; i + 1 < count; ++i) {
      result.push_back(static_cast<char>((value >> (24 - 8 * i)) & 0xff));
    }
  };

  for (const char c : input) {
    if (ObjectParser::is_whitespace(c)) {
      continue;
    }
    if (c == 'z') {
      if (n != 0) {
        throw std::runtime_error("ASCII85Decode: z inside group");
      }
      result.append(4, '\0');
      continue;
    }
    if (c == '~') {
      break;
    }
    if (c < '!' || c > 'u') {
      throw std::runtime_error("invalid character in ASCII85Decode");
    }
    digits[n++] = static_cast<std::uint32_t>(c - '!');
    if (n == 5) {
      flush(5);
      n = 0;
    }
  }
  if (n == 1) {
    throw std::runtime_error("ASCII85Decode: truncated group");
  }
  if (n > 1) {
    flush(n);
  }

  return result;
}

std::string pdf::lzw_decode(const std::string &input,
                            const Integer early_change) {
  std::string result;

  std::vector<std::string> table; // codes 258 and up
  std::string previous;
  std::uint32_t width = 9;
  std::size_t bit_position = 0;
  const std::size_t bit_end = input.size() * 8;

  const auto read_code = [&]() -> std::uint32_t {
    if (bit_position + width > bit_end) {
      return 257; // input exhausted without EOD marker: treat as EOD
    }
    std::uint32_t code = 0;
    for (std::uint32_t i = 0; i < width; ++i, ++bit_position) {
      const auto byte = static_cast<std::uint8_t>(input[bit_position >> 3]);
      code = (code << 1) | ((byte >> (7 - (bit_position & 7))) & 1);
    }
    return code;
  };

  while (true) {
    const std::uint32_t code = read_code();
    if (code == 257) {
      break;
    }
    if (code == 256) {
      table.clear();
      previous.clear();
      width = 9;
      continue;
    }

    std::string entry;
    if (code < 256) {
      entry = std::string(1, static_cast<char>(code));
    } else {
      if (previous.empty()) {
        throw std::runtime_error("LZWDecode: sequence code after clear");
      }
      const std::size_t index = code - 258;
      if (index < table.size()) {
        entry = table[index];
      } else if (index == table.size()) {
        entry = previous + previous[0];
      } else {
        throw std::runtime_error("LZWDecode: invalid code");
      }
    }

    if (!previous.empty()) {
      table.push_back(previous + entry[0]);
      const std::uint64_t next_code = 258 + table.size();
      if (next_code + early_change == 512) {
        width = 10;
      } else if (next_code + early_change == 1024) {
        width = 11;
      } else if (next_code + early_change == 2048) {
        width = 12;
      }
    }
    result += entry;
    previous = std::move(entry);
  }

  return result;
}

std::string pdf::run_length_decode(const std::string &input) {
  std::string result;

  std::size_t i = 0;
  while (i < input.size()) {
    const auto length = static_cast<std::uint8_t>(input[i++]);
    if (length == 128) {
      break;
    }
    if (length < 128) {
      if (i + length + 1 > input.size()) {
        throw std::runtime_error("RunLengthDecode: truncated input");
      }
      result.append(input, i, length + 1);
      i += length + 1;
    } else {
      if (i >= input.size()) {
        throw std::runtime_error("RunLengthDecode: truncated input");
      }
      result.append(257 - length, input[i++]);
    }
  }

  return result;
}

std::string pdf::apply_predictor(std::string data, const Integer predictor,
                                 const Integer colors,
                                 const Integer bits_per_component,
                                 const Integer columns) {
  if (predictor <= 1) {
    return data;
  }
  if (predictor == 2) {
    return apply_tiff_predictor(std::move(data), colors, bits_per_component,
                                columns);
  }
  if (predictor >= 10 && predictor <= 15) {
    return apply_png_predictor(data, colors, bits_per_component, columns);
  }
  throw std::runtime_error("unknown predictor: " + std::to_string(predictor));
}

} // namespace odr::internal
