#include <odr/internal/iwork/iwork_protobuf.hpp>

#include <odr/internal/util/byte_util.hpp>

#include <stdexcept>

namespace odr::internal {

namespace {

std::uint64_t read_fixed(const std::string_view in, std::size_t &position,
                         const std::size_t size) {
  if (position + size > in.size()) {
    throw std::runtime_error("iwork: protobuf fixed field is cut off");
  }
  const std::uint64_t result =
      util::byte::from_little_endian<std::uint64_t>(in.substr(position), size);
  position += size;
  return result;
}

} // namespace

std::uint64_t iwork::read_varint(const std::string_view in,
                                 std::size_t &position) {
  std::uint64_t result = 0;
  for (std::uint32_t shift = 0; shift <= 63; shift += 7) {
    if (position >= in.size()) {
      throw std::runtime_error("iwork: protobuf varint does not terminate");
    }
    const auto byte = static_cast<std::uint8_t>(in[position++]);
    result |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0) {
      return result;
    }
  }
  throw std::runtime_error("iwork: protobuf varint does not terminate");
}

iwork::Message::Message(const std::string_view bytes) {
  std::size_t position = 0;

  while (position < bytes.size()) {
    const std::uint64_t key = read_varint(bytes, position);
    const auto wire_type = static_cast<WireType>(key & 0x07);
    const auto number = static_cast<std::uint32_t>(key >> 3);
    if (number == 0) {
      throw std::runtime_error("iwork: protobuf field number zero");
    }

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
    }

    m_fields.push_back(field);
  }
}

const std::vector<iwork::Field> &iwork::Message::fields() const noexcept {
  return m_fields;
}

std::optional<iwork::Field>
iwork::Message::field(const std::uint32_t number) const {
  std::optional<Field> result;
  for (const Field &field : m_fields) {
    if (field.number == number) {
      result = field;
    }
  }
  return result;
}

std::vector<iwork::Field>
iwork::Message::repeated_field(const std::uint32_t number) const {
  std::vector<Field> result;
  for (const Field &field : m_fields) {
    if (field.number == number) {
      result.push_back(field);
    }
  }
  return result;
}

std::optional<std::uint64_t>
iwork::Message::number_field(const std::uint32_t number) const {
  const std::optional<Field> field = this->field(number);
  if (!field.has_value() || field->type == WireType::length_delimited) {
    return {};
  }
  return field->number_value;
}

std::optional<std::string_view>
iwork::Message::bytes_field(const std::uint32_t number) const {
  const std::optional<Field> field = this->field(number);
  if (!field.has_value() || field->type != WireType::length_delimited) {
    return {};
  }
  return field->bytes;
}

} // namespace odr::internal
