#include <odr/internal/oldms/structs/fib_rg_fc_lcb_2003.hpp>

#include <odr/internal/util/byte_stream_util.hpp>

namespace odr::internal {

static_assert(sizeof(oldms::FibRgFcLcb2003) == 1312,
              "FibRgFcLcb2003 should be 1312 bytes in size");

void oldms::read(std::istream &in, FibRgFcLcb2003 &fib_base) {
  util::byte_stream::read(in, fib_base);
}

} // namespace odr::internal
