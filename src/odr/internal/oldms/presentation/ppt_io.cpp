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

std::u16string presentation::read_raw_text_chars(std::istream &in,
                                                 const std::uint32_t rec_len) {
  // recLen MUST be even ([MS-PPT] 2.9.42); an odd one would leave the caller's
  // record cursor one byte behind the stream.
  if (rec_len % 2 != 0) {
    throw std::runtime_error("ppt: odd TextCharsAtom length " +
                             std::to_string(rec_len));
  }
  std::u16string buffer;
  buffer.resize(rec_len / 2);
  util::byte_stream::read(in, reinterpret_cast<char *>(buffer.data()), rec_len);
  return buffer;
}

std::string presentation::read_raw_text_bytes(std::istream &in,
                                              const std::uint32_t rec_len) {
  std::string buffer;
  buffer.resize(rec_len);
  in.read(buffer.data(), static_cast<std::streamsize>(rec_len));
  buffer.resize(static_cast<std::size_t>(in.gcount()));
  return buffer;
}

std::string presentation::decode_text_bytes(const std::string_view bytes) {
  std::u16string buffer;
  buffer.reserve(bytes.size());
  for (const char c : bytes) {
    buffer.push_back(static_cast<std::uint8_t>(c));
  }
  return util::string::u16string_to_string(buffer);
}

} // namespace odr::internal::oldms
