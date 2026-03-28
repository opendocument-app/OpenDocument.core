#include <odr/internal/oldms/structs/fib_rg_fc_lcb_2007.hpp>

#include <odr/internal/util/byte_stream_util.hpp>

namespace odr::internal {

static_assert(sizeof(oldms::FibRgFcLcb2007) == 1464,
              "FibRgFcLcb2007 should be 1464 bytes in size");

void oldms::read(std::istream &in, FibRgFcLcb2007 &fib_base) {
  util::byte_stream::read(in, fib_base);
}

} // namespace odr::internal
