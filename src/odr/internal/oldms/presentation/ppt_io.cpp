#include <odr/internal/oldms/presentation/ppt_io.hpp>

#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <cstdint>
#include <istream>

namespace odr::internal::oldms::presentation {

void read(std::istream &in, RecordHeader &out) {
  util::byte_stream::read(in, out);
}

void read(std::istream &in, CurrentUserAtomHead &out) {
  util::byte_stream::read(in, out);
}

void read(std::istream &in, UserEditAtomBody &out) {
  util::byte_stream::read(in, out);
}

std::uint32_t read_u32(std::istream &in) {
  return util::byte_stream::read<std::uint32_t>(in);
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

std::optional<Anchor> read_client_anchor(std::istream &in,
                                         const std::uint32_t rec_len) {
  const auto read_rect = [&in](auto tag) -> Anchor {
    using T = decltype(tag);
    Anchor anchor;
    anchor.top = util::byte_stream::read<T>(in);
    anchor.left = util::byte_stream::read<T>(in);
    anchor.right = util::byte_stream::read<T>(in);
    anchor.bottom = util::byte_stream::read<T>(in);
    return anchor;
  };

  if (rec_len == 8) {
    return read_rect(std::int16_t{}); // SmallRectStruct
  }
  if (rec_len == 16) {
    return read_rect(std::int32_t{}); // RectStruct
  }
  return std::nullopt;
}

} // namespace odr::internal::oldms::presentation
