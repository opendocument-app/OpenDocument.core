#include <odr/internal/text/text_util.hpp>

#include <odr/exceptions.hpp>

#include <array>
#include <istream>

#include <uchardet/uchardet.h>

namespace odr::internal {

std::string text::guess_charset(std::istream &in) {
  const auto ud = uchardet_new();
  std::array<char, 4096> buffer{};

  while (true) {
    in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto read = in.gcount();
    if (read == 0) {
      break;
    }
    uchardet_handle_data(ud, buffer.data(), read);
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
