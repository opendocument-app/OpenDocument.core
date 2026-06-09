#include <odr/internal/oldms/presentation/ppt_io.hpp>

#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <istream>
#include <stdexcept>
#include <string>

namespace odr::internal::oldms {

presentation::RecordHeader presentation::read_record_header(std::istream &in) {
  RecordHeader result{};
  util::byte_stream::read(in, result);
  return result;
}

presentation::CurrentUserAtomHead
presentation::read_current_user_atom_head(std::istream &in) {
  CurrentUserAtomHead result{};
  util::byte_stream::read(in, result);
  return result;
}

presentation::UserEditAtomBody
presentation::read_user_edit_atom_body(std::istream &in) {
  UserEditAtomBody result{};
  util::byte_stream::read(in, result);
  return result;
}

std::uint32_t presentation::read_u32(std::istream &in) {
  return util::byte_stream::read<std::uint32_t>(in);
}

std::string presentation::read_text_chars(std::istream &in,
                                          const std::uint32_t rec_len) {
  const std::size_t count = rec_len / 2;
  std::u16string buffer;
  buffer.resize(count);
  in.read(reinterpret_cast<char *>(buffer.data()),
          static_cast<std::streamsize>(count * sizeof(char16_t)));
  return util::string::u16string_to_string(buffer);
}

std::string presentation::read_text_bytes(std::istream &in,
                                          const std::uint32_t rec_len) {
  static constexpr auto eof = std::istream::traits_type::eof();

  std::u16string buffer;
  buffer.reserve(rec_len);
  for (std::uint32_t i = 0; i < rec_len; ++i) {
    const auto c = in.get();
    if (c == eof) {
      break;
    }
    buffer.push_back(static_cast<unsigned char>(c));
  }
  return util::string::u16string_to_string(buffer);
}

presentation::Anchor
presentation::read_client_anchor(std::istream &in,
                                 const std::uint32_t rec_len) {
  const auto read_rect = [&in]<typename T>(T) -> Anchor {
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
  throw std::runtime_error("ppt: unexpected OfficeArtClientAnchor length " +
                           std::to_string(rec_len));
}

} // namespace odr::internal::oldms
