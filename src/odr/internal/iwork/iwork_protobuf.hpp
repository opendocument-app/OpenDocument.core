#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace odr::internal::iwork {

/// Protobuf wire types; unsupported group fields are rejected.
enum class WireType : std::uint8_t {
  varint = 0,
  fixed64 = 1,
  length_delimited = 2,
  start_group = 3,
  end_group = 4,
  fixed32 = 5,
};

/// A protobuf field: numeric data in @ref number_value, length-delimited data
/// in @ref bytes.
struct Field final {
  std::uint32_t number{};
  WireType type{WireType::varint};
  std::uint64_t number_value{};
  std::string_view bytes;
};

/// A field-number view of a protobuf message. The input buffer must outlive
/// this object and its nested views.
class Message final {
public:
  explicit Message(std::string_view bytes);

  [[nodiscard]] const std::vector<Field> &fields() const noexcept;

  /// The last field numbered @p number, which is what protobuf makes of a
  /// non-repeated field appearing more than once.
  [[nodiscard]] std::optional<Field> field(std::uint32_t number) const;
  [[nodiscard]] std::vector<Field> repeated_field(std::uint32_t number) const;

  [[nodiscard]] std::optional<std::uint64_t>
  number_field(std::uint32_t number) const;
  [[nodiscard]] std::optional<std::string_view>
  bytes_field(std::uint32_t number) const;
  /// A `fixed32` field read as the `float` iWork stores geometry in.
  [[nodiscard]] std::optional<float> float_field(std::uint32_t number) const;

private:
  std::vector<Field> m_fields;
};

/// Reads a varint at @p position and advances it past the field. Throws when
/// the varint does not terminate within ten bytes.
std::uint64_t read_varint(std::string_view in, std::size_t &position);

} // namespace odr::internal::iwork
