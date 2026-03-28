#include <odr/internal/oldms/structs/fib_rg_fc_lcb_2000.hpp>

#include <odr/internal/util/byte_stream_util.hpp>

namespace odr::internal {

static_assert(sizeof(oldms::FibRgFcLcb2000) == 864,
              "FibRgFcLcb2000 should be 864 bytes in size");

void oldms::read(std::istream &in, FibRgFcLcb2000 &fib_base) {
  util::byte_stream::read(in, fib_base);
}

} // namespace odr::internal
