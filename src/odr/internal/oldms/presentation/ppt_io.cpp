#include <odr/internal/oldms/presentation/ppt_io.hpp>

#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <istream>

namespace odr::internal::oldms::presentation {

void read(std::istream &in, RecordHeader &out) {
  util::byte_stream::read(in, out);
}

std::string read_text_chars(std::istream &in, const std::uint32_t rec_len) {
  const std::size_t count = rec_len / 2;
  std::u16string buffer;
  buffer.resize(count);
  in.read(reinterpret_cast<char *>(buffer.data()),
          static_cast<std::streamsize>(count * sizeof(char16_t)));
  return util::string::u16string_to_string(buffer);
}

std::string read_text_bytes(std::istream &in, const std::uint32_t rec_len) {
  static constexpr auto eof = std::istream::traits_type::eof();

  std::u16string buffer;
  buffer.reserve(rec_len);
  for (std::uint32_t i = 0; i < rec_len; ++i) {
    const auto c = in.get();
    if (c == eof) {
      break;
    }
    buffer.push_back(static_cast<char16_t>(static_cast<unsigned char>(c)));
  }
  return util::string::u16string_to_string(buffer);
}

} // namespace odr::internal::oldms::presentation
