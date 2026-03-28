#include <odr/internal/oldms/word/io.hpp>

#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/stream_util.hpp>

namespace odr::internal {

void oldms::read(std::istream &in, FibRgFcLcb97 &fib_base) {
  util::byte_stream::read(in, fib_base);
}

void oldms::read(std::istream &in, FibRgFcLcb2000 &fib_base) {
  util::byte_stream::read(in, fib_base);
}

void oldms::read(std::istream &in, FibRgFcLcb2002 &fib_base) {
  util::byte_stream::read(in, fib_base);
}

void oldms::read(std::istream &in, FibRgFcLcb2003 &fib_base) {
  util::byte_stream::read(in, fib_base);
}

void oldms::read(std::istream &in, FibRgFcLcb2007 &fib_base) {
  util::byte_stream::read(in, fib_base);
}

void oldms::read(std::istream &in, Fib &fib) {
  read(in, fib.base);

  util::byte_stream::read(in, fib.csw);
  if (fib.csw * 2 < sizeof(fib.fibRgW)) {
    throw std::runtime_error("Unexpected Fib.csw value: " +
                             std::to_string(fib.csw));
  }

  const std::streampos offsetFibRgW = in.tellg();
  util::byte_stream::read(in, fib.fibRgW);
  in.seekg(offsetFibRgW + static_cast<std::streamoff>(fib.csw * 2));

  util::byte_stream::read(in, fib.cslw);
  if (fib.cslw * 4 < sizeof(fib.fibRgLw)) {
    throw std::runtime_error("Unexpected Fib.cslw value: " +
                             std::to_string(fib.cslw));
  }

  const std::streampos offsetFibRgLw = in.tellg();
  util::byte_stream::read(in, fib.fibRgLw);
  in.seekg(offsetFibRgLw + static_cast<std::streamoff>(fib.cslw * 4));

  util::byte_stream::read(in, fib.cbRgFcLcb);

  const std::streampos offsetFibRgFcLcb = in.tellg();
  in.seekg(offsetFibRgFcLcb + static_cast<std::streamoff>(fib.cbRgFcLcb * 8));

  util::byte_stream::read(in, fib.cswNew);

  const std::streampos offsetFibRgCswNew = in.tellg();
  read(in, fib.fibRgCswNew);
  in.seekg(offsetFibRgCswNew + static_cast<std::streamoff>(fib.cswNew * 2));

  const std::uint16_t nFib =
      (fib.cswNew != 0) ? fib.fibRgCswNew.nFibNew : fib.base.nFib;

  in.seekg(offsetFibRgFcLcb);
  fib.fibRgFcLcb = readFibRgFcLcb(in, nFib);
}

void oldms::read(std::istream &in, FibBase &fibBase) {
  util::byte_stream::read(in, fibBase);
}

void oldms::read(std::istream &in, FibRgCswNew &out) {
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
oldms::readFibRgFcLcb(std::istream &in, const std::uint16_t nFib) {
  switch (nFib) {
  case nFib97: {
    auto result = std::make_unique<FibRgFcLcb97>();
    read(in, *result);
    return result;
  }
  case nFib2000: {
    auto result = std::make_unique<FibRgFcLcb2000>();
    read(in, *result);
    return result;
  }
  case nFib2002: {
    auto result = std::make_unique<FibRgFcLcb2002>();
    read(in, *result);
    return result;
  }
  case nFib2003: {
    auto result = std::make_unique<FibRgFcLcb2003>();
    read(in, *result);
    return result;
  }
  case nFib2007: {
    auto result = std::make_unique<FibRgFcLcb2007>();
    read(in, *result);
    return result;
  }
  default:
    throw std::runtime_error("Unknown nFib value: " + std::to_string(nFib));
  }
}

} // namespace odr::internal
