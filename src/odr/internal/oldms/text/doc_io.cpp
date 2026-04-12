#include <odr/internal/oldms/text/doc_io.hpp>

#include "odr/internal/util/string_util.hpp"

#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

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
        if (sizeof(FibRgFcLcbType) < out.cbRgFcLcb * 8) {
          throw std::runtime_error("Unexpected cbRgFcLcb value: " +
                                   std::to_string(out.cbRgFcLcb));
        }
        return std::unique_ptr<FibRgFcLcb97>(
            reinterpret_cast<FibRgFcLcb97 *>(fibRgFcLcb.release()));
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

std::string text::read_string_compressed(std::istream &in,
                                         const std::size_t size) {
  static constexpr auto eof = std::istream::traits_type::eof();

  std::string result;
  result.reserve(size);

  for (std::size_t i = 0; i < size; ++i) {
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
      result.push_back(c);
    }
  }

  return result;
}

std::u16string text::read_string_uncompressed(std::istream &in,
                                              const std::size_t size) {
  std::u16string result;
  result.resize(size);

  in.read(reinterpret_cast<char *>(result.data()),
          static_cast<std::streamsize>(size * sizeof(char16_t)));

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
