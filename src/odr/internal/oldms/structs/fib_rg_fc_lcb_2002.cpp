#include <odr/internal/oldms/structs/fib_rg_fc_lcb_2002.hpp>

#include <odr/internal/util/byte_stream_util.hpp>

namespace odr::internal {

static_assert(sizeof(oldms::FibRgFcLcb2002) == 1088,
              "FibRgFcLcb2002 should be 1088 bytes in size");

void oldms::read(std::istream &in, FibRgFcLcb2002 &fib_base) {
  util::byte_stream::read(in, fib_base);
}

} // namespace odr::internal
