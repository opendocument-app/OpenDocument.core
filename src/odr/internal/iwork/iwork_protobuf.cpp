#include <odr/internal/iwork/iwork_protobuf.hpp>

#include <odr/internal/util/byte_util.hpp>

#include <bit>
#include <stdexcept>

namespace odr::internal::iwork {

namespace {

std::uint64_t read_fixed(const std::string_view in, std::size_t &position,
                         const std::size_t size) {
  if (position > in.size() || size > in.size() - position) {
    throw std::runtime_error("iwork: protobuf fixed field is cut off");
  }
  const std::uint64_t result =
      util::byte::from_little_endian<std::uint64_t>(in.substr(position), size);
  position += size;
  return result;
}

} // namespace

Message::Message(const std::string_view bytes) {
  std::size_t position = 0;

  while (position < bytes.size()) {
    const std::uint64_t key = read_varint(bytes, position);
    const auto wire_type = static_cast<WireType>(key & 0x07);
    if ((key >> 3) == 0 || (key >> 3) > 0x1fffffff) {
      throw std::runtime_error("iwork: invalid protobuf field number");
    }
    const auto number = static_cast<std::uint32_t>(key >> 3);

    Field field;
    field.number = number;
    field.type = wire_type;

    switch (wire_type) {
    case WireType::varint:
      field.number_value = read_varint(bytes, position);
      break;
    case WireType::fixed64:
      field.number_value = read_fixed(bytes, position, 8);
      break;
    case WireType::fixed32:
      field.number_value = read_fixed(bytes, position, 4);
      break;
    case WireType::length_delimited: {
      const std::uint64_t length = read_varint(bytes, position);
      if (length > bytes.size() - position) {
        throw std::runtime_error("iwork: protobuf field runs past the message");
      }
      field.bytes = bytes.substr(position, length);
      position += length;
    } break;
    case WireType::start_group:
    case WireType::end_group:
      throw std::runtime_error("iwork: protobuf group field");
    default:
      throw std::runtime_error("iwork: invalid protobuf wire type");
    }

    m_fields.push_back(field);
  }
}

const std::vector<Field> &Message::fields() const noexcept { return m_fields; }

std::optional<Field> Message::field(const std::uint32_t number) const {
  std::optional<Field> result;
  for (const Field &field : m_fields) {
    if (field.number == number) {
      result = field;
    }
  }
  return result;
}

std::vector<Field> Message::repeated_field(const std::uint32_t number) const {
  std::vector<Field> result;
  for (const Field &field : m_fields) {
    if (field.number == number) {
      result.push_back(field);
    }
  }
  return result;
}

std::optional<std::uint64_t>
Message::number_field(const std::uint32_t number) const {
  const std::optional<Field> field = this->field(number);
  if (!field.has_value() || field->type == WireType::length_delimited) {
    return {};
  }
  return field->number_value;
}

std::optional<std::string_view>
Message::bytes_field(const std::uint32_t number) const {
  const std::optional<Field> field = this->field(number);
  if (!field.has_value() || field->type != WireType::length_delimited) {
    return {};
  }
  return field->bytes;
}

std::optional<float> Message::float_field(const std::uint32_t number) const {
  const std::optional<Field> field = this->field(number);
  if (!field.has_value() || field->type != WireType::fixed32) {
    return {};
  }
  return std::bit_cast<float>(static_cast<std::uint32_t>(field->number_value));
}

} // namespace odr::internal::iwork

namespace odr::internal {

std::uint64_t iwork::read_varint(const std::string_view in,
                                 std::size_t &position) {
  std::uint64_t result = 0;
  for (std::uint32_t shift = 0; shift <= 63; shift += 7) {
    if (position >= in.size()) {
      throw std::runtime_error("iwork: protobuf varint does not terminate");
    }
    const auto byte = static_cast<std::uint8_t>(in[position++]);
    if (shift == 63 && byte > 1) {
      throw std::runtime_error("iwork: protobuf varint overflows");
    }
    result |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0) {
      return result;
    }
  }
  throw std::runtime_error("iwork: protobuf varint does not terminate");
}

} // namespace odr::internal
