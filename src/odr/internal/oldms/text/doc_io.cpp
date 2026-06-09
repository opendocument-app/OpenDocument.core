#include <odr/internal/oldms/text/doc_io.hpp>

#include "odr/internal/util/string_util.hpp"

#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <cstring>

namespace odr::internal::oldms::text {

namespace {

template <typename T> struct TypeTag {
  using type = T;
};

template <typename F>
auto type_dispatch_FibRgFcLcb(const std::uint16_t nFib, const F &f) {
  switch (nFib) {
  case nFib97: {
    return f(TypeTag<FibRgFcLcb97>{});
  }
  case nFib2000: {
    return f(TypeTag<FibRgFcLcb2000>{});
  }
  case nFib2002: {
    return f(TypeTag<FibRgFcLcb2002>{});
  }
  case nFib2003: {
    return f(TypeTag<FibRgFcLcb2003>{});
  }
  case nFib2007: {
    return f(TypeTag<FibRgFcLcb2007>{});
  }
  default:
    // A newer-than-2007 FIB only appends FcLcb entries; the fields we read
    // ([MS-DOC] fcClx) live in the FibRgFcLcb97 base, so reuse the newest
    // layout we model and let the caller ignore the surplus entries. Genuinely
    // older / unrecognised versions (< nFib97) still fail.
    if (nFib > nFib2007) {
      return f(TypeTag<FibRgFcLcb2007>{});
    }
    throw std::runtime_error("Unknown nFib value: " + std::to_string(nFib));
  }
}

} // namespace

} // namespace odr::internal::oldms::text

namespace odr::internal::oldms {

void text::read(std::istream &in, FibBase &out) {
  util::byte_stream::read(in, out);
}

void text::read(std::istream &in, FibRgFcLcb97 &out) {
  util::byte_stream::read(in, out);
}

void text::read(std::istream &in, FibRgFcLcb2000 &out) {
  util::byte_stream::read(in, out);
}

void text::read(std::istream &in, FibRgFcLcb2002 &out) {
  util::byte_stream::read(in, out);
}

void text::read(std::istream &in, FibRgFcLcb2003 &out) {
  util::byte_stream::read(in, out);
}

void text::read(std::istream &in, FibRgFcLcb2007 &out) {
  util::byte_stream::read(in, out);
}

std::size_t text::determine_size_Fib(std::istream &in) {
  std::size_t result = 0;

  const auto read_uint16_t = [&] {
    const std::uint16_t value = util::byte_stream::read<std::uint16_t>(in);
    result += sizeof(std::uint16_t);
    return value;
  };
  const auto ignore = [&](const std::size_t count) {
    in.ignore(count);
    result += count;
  };

  ignore(sizeof(FibBase));
  const std::uint16_t csw = read_uint16_t();
  ignore(csw * 2);
  const std::uint16_t cslw = read_uint16_t();
  ignore(cslw * 4);
  const std::uint16_t cbRgFcLcb = read_uint16_t();
  ignore(cbRgFcLcb * 8);
  const std::uint16_t cswNew = read_uint16_t();
  ignore(cswNew * 2);

  return result;
}

void text::read(std::istream &in, ParsedFib &out) {
  read(in, out.base);

  util::byte_stream::read(in, out.csw);
  if (out.csw * 2 < sizeof(out.fibRgW)) {
    throw std::runtime_error("Unexpected Fib.csw value: " +
                             std::to_string(out.csw));
  }
  util::byte_stream::read(in, out.fibRgW);
  in.ignore(out.csw * 2 - sizeof(out.fibRgW));

  util::byte_stream::read(in, out.cslw);
  if (out.cslw * 4 < sizeof(out.fibRgLw)) {
    throw std::runtime_error("Unexpected Fib.cslw value: " +
                             std::to_string(out.cslw));
  }
  util::byte_stream::read(in, out.fibRgLw);
  in.ignore(out.cslw * 4 - sizeof(out.fibRgLw));

  // ccpText ([MS-DOC] 2.5.5 FibRgLw97) is a signed integer that MUST be >= 0.
  // We assemble it unsigned (ParsedFib::ccpText); reject a value with the sign
  // bit set as malformed rather than treating it as a huge length.
  if (out.ccpText() < 0) {
    throw std::runtime_error("Unexpected negative Fib.ccpText: " +
                             std::to_string(out.ccpText()));
  }

  util::byte_stream::read(in, out.cbRgFcLcb);
  auto fibRgFcLcb = std::make_unique<char[]>(out.cbRgFcLcb * 8);
  in.read(fibRgFcLcb.get(), out.cbRgFcLcb * 8);

  util::byte_stream::read(in, out.cswNew);
  if (out.cswNew > 0) {
    out.fibRgCswNew.emplace();
    read(in, *out.fibRgCswNew);
  }
  const std::uint16_t nFib =
      out.cswNew != 0 ? out.fibRgCswNew.value().nFibNew : out.base.nFib;

  out.fibRgFcLcb = type_dispatch_FibRgFcLcb(
      nFib, [&]<typename T>(const T) -> std::unique_ptr<FibRgFcLcb97> {
        using FibRgFcLcbType = T::type;
        auto result = std::make_unique<FibRgFcLcbType>();
        // Copy only what fits: a newer FIB carries more FcLcb entries than the
        // modelled layout (ignore the surplus), an older one carries fewer
        // (leave the remainder zero-initialised). The fcClx we need lives in
        // the FibRgFcLcb97 base, so it is always covered.
        const std::size_t copy =
            std::min<std::size_t>(sizeof(FibRgFcLcbType), out.cbRgFcLcb * 8);
        std::memcpy(result.get(), fibRgFcLcb.get(), copy);
        return result;
      });
}

void text::read(std::istream &in, ParsedFibRgCswNew &out) {
  util::byte_stream::read(in, out.nFibNew);

  switch (out.nFibNew) {
  case nFib97:
    break;
  case nFib2000:
  case nFib2002:
  case nFib2003: {
    auto rgCswNewData = std::make_unique<FibRgCswNewData2000>();
    util::byte_stream::read(in, *rgCswNewData);
    out.rgCswNewData = std::move(rgCswNewData);
  } break;
  case nFib2007: {
    auto rgCswNewData = std::make_unique<FibRgCswNewData2007>();
    util::byte_stream::read(in, *rgCswNewData);
    out.rgCswNewData = std::move(rgCswNewData);
  } break;
  default:
    throw std::runtime_error("Unsupported nFibNew value: " +
                             std::to_string(out.nFibNew));
  }
}

std::unique_ptr<text::FibRgFcLcb97>
text::read_FibRgFcLcb(std::istream &in, const std::uint16_t nFib) {
  return type_dispatch_FibRgFcLcb(
      nFib, [&in]<typename T>(const T) -> std::unique_ptr<FibRgFcLcb97> {
        using FibRgFcLcbType = T::type;
        auto result = std::make_unique<FibRgFcLcbType>();
        read(in, *result);
        return result;
      });
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

  const std::uint16_t cbGrpprl = util::byte_stream::read<std::uint16_t>(in);
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
      // Compressed text is an array of 8-bit Unicode characters ([MS-DOC]
      // 2.9.73 / 2.4.1 step 6): a byte that is not one of the mapped values
      // denotes code point U+00XX, so UTF-8-encode it. Emitting the raw byte
      // would be correct only for 0x00-0x7F and produce invalid UTF-8 for
      // 0xA0-0xFF (and the unmapped 0x80/0x81/0x8D/0x8E/0x8F/0x90).
      util::string::append_c32(static_cast<char32_t>(ci), result);
    }
  }

  return result;
}

std::u16string text::read_string_uncompressed(std::istream &in,
                                              const std::size_t length_cp) {
  std::u16string result;
  result.resize(length_cp);

  in.read(reinterpret_cast<char *>(result.data()),
          static_cast<std::streamsize>(length_cp * sizeof(char16_t)));

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
