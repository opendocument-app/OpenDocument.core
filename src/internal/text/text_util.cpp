#include <internal/text/text_util.h>
#include <internal/util/stream_util.h>
#include <iostream>
#include <odr/exceptions.h>
#include <uchardet.h>

namespace odr::internal {

std::string text::guess_charset(std::istream &in) {
  static constexpr auto BUFFER_SIZE = 4096;

  auto ud = uchardet_new();
  char buffer[BUFFER_SIZE];

  while (true) {
    in.read(buffer, BUFFER_SIZE);
    const auto read = in.gcount();
    if (read == 0) {
      break;
    }
    uchardet_handle_data(ud, buffer, read);
  }

  uchardet_data_end(ud);
  std::string result = uchardet_get_charset(ud);
  uchardet_delete(ud);

  if (result.empty()) {
    throw UnknownCharset();
  }
  return result;
}

} // namespace odr::internal
