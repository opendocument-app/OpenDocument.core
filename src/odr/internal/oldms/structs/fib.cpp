#include <odr/internal/oldms/structs/fib.hpp>

#include <odr/internal/oldms/structs/fib_rg_fc_lcb.hpp>
#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/stream_util.hpp>

namespace odr::internal {

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
  fib.fibRgFcLcb = read_fib_rg_fc_lcb(in, nFib);
}

} // namespace odr::internal
