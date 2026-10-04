#include <odr/internal/oldms/text/doc_io.hpp>

#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/string_util.hpp>

#include <odr/internal/util/byte_stream_util.hpp>

#include <algorithm>
#include <cstring>
#include <iostream>

namespace odr::internal::oldms::text {

namespace {

void validate_version(const std::uint16_t version, const bool allow_future) {
  switch (version) {
  case nFib97:
  case nFib2000:
  case nFib2002:
  case nFib2003:
  case nFib2007:
    return;
  default:
    if (allow_future && version > nFib2007) {
      return;
    }
    throw std::runtime_error("doc: unsupported FIB version " +
                             std::to_string(version));
  }
}

} // namespace

} // namespace odr::internal::oldms::text

namespace odr::internal::oldms {

void text::read(std::istream &in, FibBase &out) {
  util::byte_stream::read(in, out);
}

void text::read(std::istream &in, ParsedFib &out) {
  read(in, out.base);
  if (out.base.wIdent != fib_wIdent) {
    throw std::runtime_error("doc: invalid FIB signature");
  }

  util::byte_stream::read(in, out.csw);
  if (static_cast<std::size_t>(out.csw) * 2 < sizeof(out.fibRgW)) {
    throw std::runtime_error("Unexpected Fib.csw value: " +
                             std::to_string(out.csw));
  }
  util::byte_stream::read(in, out.fibRgW);
  in.ignore(static_cast<std::streamsize>(static_cast<std::size_t>(out.csw) * 2 -
                                         sizeof(out.fibRgW)));

  util::byte_stream::read(in, out.cslw);
  if (static_cast<std::size_t>(out.cslw) * 4 < sizeof(out.fibRgLw)) {
    throw std::runtime_error("Unexpected Fib.cslw value: " +
                             std::to_string(out.cslw));
  }
  util::byte_stream::read(in, out.fibRgLw);
  in.ignore(static_cast<std::streamsize>(
      static_cast<std::size_t>(out.cslw) * 4 - sizeof(out.fibRgLw)));

  // ccpText MUST be >= 0 ([MS-DOC] 2.5.5).
  if (out.ccpText() < 0) {
    throw std::runtime_error("Unexpected negative Fib.ccpText: " +
                             std::to_string(out.ccpText()));
  }

  util::byte_stream::read(in, out.cbRgFcLcb);
  const std::string offsets = util::byte_stream::read_u8s(
      in, std::uint32_t{out.cbRgFcLcb} * sizeof(FcLcb));
  out.fibRgFcLcb = {};
  std::memcpy(&out.fibRgFcLcb, offsets.data(),
              std::min(sizeof(out.fibRgFcLcb), offsets.size()));

  util::byte_stream::read(in, out.cswNew);
  out.nFibNew.reset();
  if (out.cswNew > 0) {
    const std::string extension = util::byte_stream::read_u8s(
        in, std::uint32_t{out.cswNew} * sizeof(std::uint16_t));
    util::byte_string::Reader cursor(extension);
    out.nFibNew = cursor.read<std::uint16_t>();
    validate_version(*out.nFibNew, false);
    // [MS-DOC] 2.5.11–13: only the version affects the fields we use.
    cursor.skip(*out.nFibNew == nFib97 ? 0
                                       : (*out.nFibNew == nFib2007 ? 8 : 2));
  }
  validate_version(out.nFibNew.value_or(out.base.nFib),
                   !out.nFibNew.has_value());
}

void text::read_Clx(std::istream &in, const HandlePrc &handle_Prc,
                    const HandlePcdt &handle_Pcdt) {
  while (true) {
    const int c = in.peek();
    if (c == 0x2) {
      handle_Pcdt(in);
      return;
    }
    if (c != 0x1) {
      throw std::runtime_error("Unexpected input: " + std::to_string(c));
    }
    handle_Prc(in);
  }
}

void text::skip_Prc(std::istream &in) {
  if (const int c = in.get(); c != 0x1) {
    throw std::runtime_error("Unexpected input: " + std::to_string(c));
  }

  const auto cbGrpprl = util::byte_stream::read<std::uint16_t>(in);
  in.ignore(cbGrpprl);
}

std::string text::read_string(std::istream &in, const std::size_t length_cp,
                              const bool is_compressed) {
  if (is_compressed) {
    return read_string_compressed(in, length_cp);
  }

  return util::string::u16string_to_string(
      read_string_uncompressed(in, length_cp));
}

std::string text::read_string_compressed(std::istream &in,
                                         const std::size_t length_cp) {
  static constexpr auto eof = std::istream::traits_type::eof();

  std::string result;
  result.reserve(length_cp);

  for (std::size_t i = 0; i < length_cp; ++i) {
    const auto ci = in.get();
    if (ci == eof) {
      throw std::runtime_error("Unexpected end of input");
    }
    if (ci < 0 || ci > 0xFF) {
      throw std::runtime_error("Unexpected input: " + std::to_string(ci));
    }
    const char c = static_cast<char>(ci);
    if (const std::optional<char16_t> uncompressed = uncompress_char(c);
        uncompressed.has_value()) {
      util::string::append_c32(*uncompressed, result);
    } else {
      // An unmapped byte denotes code point U+00XX ([MS-DOC] 2.4.1 step 6).
      util::string::append_c32(static_cast<char32_t>(ci), result);
    }
  }

  return result;
}

std::u16string text::read_string_uncompressed(std::istream &in,
                                              const std::size_t length_cp) {
  std::u16string result;
  result.resize(length_cp);

  util::byte_stream::read(in, reinterpret_cast<char *>(result.data()),
                          length_cp * sizeof(char16_t));

  return result;
}

std::optional<char16_t> text::uncompress_char(const char c) {
  switch (c) {
  case '\x82':
    return 0x201A;
  case '\x83':
    return 0x0192;
  case '\x84':
    return 0x201E;
  case '\x85':
    return 0x2026;
  case '\x86':
    return 0x2020;
  case '\x87':
    return 0x2021;
  case '\x88':
    return 0x02C6;
  case '\x89':
    return 0x2030;
  case '\x8A':
    return 0x0160;
  case '\x8B':
    return 0x2039;
  case '\x8C':
    return 0x0152;
  case '\x91':
    return 0x2018;
  case '\x92':
    return 0x2019;
  case '\x93':
    return 0x201C;
  case '\x94':
    return 0x201D;
  case '\x95':
    return 0x2022;
  case '\x96':
    return 0x2013;
  case '\x97':
    return 0x2014;
  case '\x98':
    return 0x02DC;
  case '\x99':
    return 0x2122;
  case '\x9A':
    return 0x0161;
  case '\x9B':
    return 0x203A;
  case '\x9C':
    return 0x0153;
  case '\x9F':
    return 0x0178;
  default:
    return std::nullopt;
  }
}

} // namespace odr::internal::oldms
