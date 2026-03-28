#include <odr/internal/oldms/structs/fib_rg_csw_new.hpp>

#include "n_fib_values.hpp"

#include <odr/internal/util/byte_stream_util.hpp>

namespace odr::internal {

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

} // namespace odr::internal
