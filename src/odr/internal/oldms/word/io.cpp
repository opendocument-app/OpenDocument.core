#include <odr/internal/oldms/word/io.hpp>

#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/stream_util.hpp>

namespace odr::internal::oldms {

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

} // namespace odr::internal::oldms

namespace odr::internal {

void oldms::read(std::istream &in, FibBase &out) {
  util::byte_stream::read(in, out);
}

void oldms::read(std::istream &in, FibRgFcLcb97 &out) {
  util::byte_stream::read(in, out);
}

void oldms::read(std::istream &in, FibRgFcLcb2000 &out) {
  util::byte_stream::read(in, out);
}

void oldms::read(std::istream &in, FibRgFcLcb2002 &out) {
  util::byte_stream::read(in, out);
}

void oldms::read(std::istream &in, FibRgFcLcb2003 &out) {
  util::byte_stream::read(in, out);
}

void oldms::read(std::istream &in, FibRgFcLcb2007 &out) {
  util::byte_stream::read(in, out);
}

std::size_t oldms::determine_size_Fib(std::istream &in) {
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

void oldms::read(std::istream &in, ParsedFib &out) {
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
  read(in, out.fibRgCswNew);

  const std::uint16_t nFib =
      out.cswNew != 0 ? out.fibRgCswNew.nFibNew : out.base.nFib;

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

void oldms::read(std::istream &in, ParsedFibRgCswNew &out) {
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

std::unique_ptr<oldms::FibRgFcLcb97>
oldms::read_FibRgFcLcb(std::istream &in, const std::uint16_t nFib) {
  return type_dispatch_FibRgFcLcb(
      nFib, [&in]<typename T>(const T) -> std::unique_ptr<FibRgFcLcb97> {
        using FibRgFcLcbType = T::type;
        auto result = std::make_unique<FibRgFcLcbType>();
        read(in, *result);
        return result;
      });
}

void oldms::read_Clx(std::istream &in, const HandlePrc &handle_Prc,
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

void oldms::skip_Prc(std::istream &in) {
  if (const int c = in.get(); c != 0x1) {
    throw std::runtime_error("Unexpected input: " + std::to_string(c));
  }

  const std::uint16_t cbGrpprl = util::byte_stream::read<std::uint16_t>(in);
  in.ignore(cbGrpprl);
}

} // namespace odr::internal
