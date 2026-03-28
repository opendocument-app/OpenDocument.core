#include <odr/internal/oldms/structs/fib_base.hpp>

#include <odr/internal/util/byte_stream_util.hpp>

namespace odr::internal {

void oldms::read(std::istream &in, FibBase &fibBase) {
  util::byte_stream::read(in, fibBase);
}

} // namespace odr::internal
