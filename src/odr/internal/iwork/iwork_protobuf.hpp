#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace odr::internal::iwork {

/// The protobuf wire types. Groups (3 and 4) are deprecated and never appear
/// in an iWork archive, so reading one is a parse error rather than a field to
/// skip.
enum class WireType : std::uint8_t {
  varint = 0,
  fixed64 = 1,
  length_delimited = 2,
  start_group = 3,
  end_group = 4,
  fixed32 = 5,
};

/// One field of a protobuf message. @ref number_value carries a varint or a
/// fixed-width field, @ref bytes a length-delimited one — a nested message, a
/// string or a packed repeated field.
struct Field final {
  std::uint32_t number{};
  WireType type{WireType::varint};
  std::uint64_t number_value{};
  std::string_view bytes;
};

/// A protobuf message read by field number: there are no schemas to generate
/// accessors from, so the archives are read against hand-written ones.
///
/// Nested messages stay as views into the buffer the message was read from,
/// which has to outlive it.
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

private:
  std::vector<Field> m_fields;
};

/// Reads a varint at @p position and advances it past the field. Throws when
/// the varint does not terminate within ten bytes.
std::uint64_t read_varint(std::string_view in, std::size_t &position);

} // namespace odr::internal::iwork
