#include <odr/internal/iwork/iwork_table.hpp>

#include <odr/internal/iwork/iwork_archive.hpp>
#include <odr/internal/iwork/iwork_budget.hpp>
#include <odr/internal/iwork/iwork_protobuf.hpp>
#include <odr/internal/iwork/iwork_types.hpp>
#include <odr/internal/util/byte_util.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace odr::internal::iwork {

namespace {

/// The bias an IEEE 754 decimal128 exponent carries.
constexpr std::int32_t decimal128_bias = 6176;

/// Divides the 128-bit @p value, four little-endian 32-bit limbs, by ten.
/// \return the remainder.
std::uint32_t divide_by_ten(std::array<std::uint32_t, 4> &value) {
  std::uint64_t remainder = 0;
  for (std::size_t i = value.size(); i-- > 0;) {
    const std::uint64_t current = (remainder << 32) | value[i];
    value[i] = static_cast<std::uint32_t>(current / 10);
    remainder = current % 10;
  }
  return static_cast<std::uint32_t>(remainder);
}

bool is_zero(const std::array<std::uint32_t, 4> &value) {
  return std::ranges::all_of(
      value, [](const std::uint32_t limb) { return limb == 0; });
}

std::uint64_t read_uint64(const std::string_view bytes,
                          const std::size_t offset) {
  return util::byte::from_little_endian<std::uint64_t>(bytes.substr(offset), 8);
}

std::uint32_t read_uint32(const std::string_view bytes,
                          const std::size_t offset) {
  return util::byte::from_little_endian<std::uint32_t>(bytes.substr(offset), 4);
}

double read_double(const std::string_view bytes, const std::size_t offset) {
  return std::bit_cast<double>(read_uint64(bytes, offset));
}

/// One `TST.DataList`, as key → entry. The entries are read once per table
/// because a cell names its value by key and the keys are not dense.
std::unordered_map<std::uint64_t, Message>
read_data_list(Package &package,
               const std::optional<std::uint64_t> identifier) {
  std::unordered_map<std::uint64_t, Message> result;
  if (!identifier.has_value()) {
    return result;
  }

  const Object &object = package.object(*identifier);
  if (object.type != archive_type::data_list) {
    return result;
  }

  for (const Field &entry :
       Message(object.payload).repeated_field(data_list::entries)) {
    if (entry.type != WireType::length_delimited) {
      throw std::runtime_error("iwork: malformed data list");
    }
    Message message(entry.bytes);
    const std::optional<std::uint64_t> key =
        message.number_field(data_list_entry::key);
    if (!key.has_value()) {
      continue;
    }
    result.emplace(*key, std::move(message));
  }
  return result;
}

/// Where each of the value flags puts its payload, and how wide it is. A cell
/// holds one value, so the walk stops once the flag the type asks for is
/// found; the offsets before it still have to be stepped over.
struct Value final {
  std::uint32_t flag{};
  std::size_t size{};
};

constexpr std::array<Value, 5> value_layout{{
    {cell::flag::decimal, 16},
    {cell::flag::number, 8},
    {cell::flag::seconds, 8},
    {cell::flag::string_key, 4},
    {cell::flag::rich_text_key, 4},
}};

/// The largest instant either formatter reads, in seconds. `std::chrono::year`
/// runs to ±32767, so a second beyond this names no date a calendar has —
/// and the `std::int64_t` the seconds are cast to holds this with room to
/// spare, which is what keeps the cast defined.
constexpr double max_instant = 1e12;

/// The offset of the value @p flag names within @p record, or nothing when the
/// flags do not carry it or it would run past the record.
std::optional<std::size_t> value_offset(const std::string_view record,
                                        const std::uint32_t flags,
                                        const std::uint32_t flag) {
  std::size_t offset = cell::header_size;
  for (const auto &[candidate, size] : value_layout) {
    if ((flags & candidate) == 0) {
      continue;
    }
    if (candidate == flag) {
      return offset + size <= record.size() ? std::optional(offset)
                                            : std::nullopt;
    }
    offset += size;
  }
  return {};
}

/// Reads one packed cell record. A type or a version we have not mapped comes
/// back empty — there is no spec, so it is a shape Apple ships and we have not
/// seen, not a corrupt file.
TableModel::Cell
read_cell(const std::string_view record, const std::uint32_t row,
          const std::uint32_t column,
          const std::unordered_map<std::uint64_t, Message> &strings,
          const std::unordered_map<std::uint64_t, Message> &rich_texts) {
  TableModel::Cell result;
  result.row = row;
  result.column = column;

  if (record.size() < cell::header_size ||
      static_cast<std::uint8_t>(record[cell::version_offset]) !=
          cell::version) {
    return result;
  }

  const auto type = static_cast<std::uint8_t>(record[cell::type_offset]);
  const std::uint32_t flags = read_uint32(record, cell::flags_offset);

  const auto key_at =
      [&](const std::uint32_t flag) -> std::optional<std::uint64_t> {
    const std::optional<std::size_t> offset = value_offset(record, flags, flag);
    if (!offset.has_value()) {
      return {};
    }
    return read_uint32(record, *offset);
  };
  const auto number_at =
      [&](const std::uint32_t flag) -> std::optional<double> {
    const std::optional<std::size_t> offset = value_offset(record, flags, flag);
    if (!offset.has_value()) {
      return {};
    }
    return read_double(record, *offset);
  };

  switch (type) {
  case cell::type::number: {
    const std::optional<std::size_t> offset =
        value_offset(record, flags, cell::flag::decimal);
    if (!offset.has_value()) {
      break;
    }
    result.text = decimal128_to_string(record.substr(*offset, 16));
    result.value_type = ValueType::float_number;
  } break;
  case cell::type::string: {
    const std::optional<std::uint64_t> key = key_at(cell::flag::string_key);
    if (!key.has_value()) {
      break;
    }
    const auto it = strings.find(*key);
    if (it == strings.end()) {
      break;
    }
    result.text = std::string(it->second.bytes_field(data_list_entry::string)
                                  .value_or(std::string_view()));
    result.value_type = ValueType::string;
  } break;
  case cell::type::date: {
    const std::optional<double> seconds = number_at(cell::flag::seconds);
    if (!seconds.has_value()) {
      break;
    }
    result.text = date_to_string(*seconds);
    result.value_type = ValueType::string;
  } break;
  case cell::type::boolean: {
    const std::optional<double> value = number_at(cell::flag::number);
    if (!value.has_value()) {
      break;
    }
    result.text = *value != 0.0 ? "TRUE" : "FALSE";
    result.value_type = ValueType::string;
  } break;
  case cell::type::duration: {
    const std::optional<double> seconds = number_at(cell::flag::number);
    if (!seconds.has_value()) {
      break;
    }
    result.text = duration_to_string(*seconds);
    result.value_type = ValueType::string;
  } break;
  case cell::type::rich_text: {
    const std::optional<std::uint64_t> key = key_at(cell::flag::rich_text_key);
    if (!key.has_value()) {
      break;
    }
    const auto it = rich_texts.find(*key);
    if (it == rich_texts.end()) {
      break;
    }
    result.storage_identifier =
        reference_identifier(it->second, data_list_entry::rich_text);
    result.value_type = ValueType::string;
  } break;
  default:
    break;
  }

  return result;
}

/// Reads the cells of one `TST.Tile`, whose row indices are relative to the
/// tile's own start.
void read_tile(Package &package, Budget &budget, const std::uint64_t identifier,
               const std::uint32_t first_row,
               const std::unordered_map<std::uint64_t, Message> &strings,
               const std::unordered_map<std::uint64_t, Message> &rich_texts,
               std::vector<TableModel::Cell> &out) {
  const Object &object = package.object(identifier);
  if (object.type != archive_type::tile) {
    return;
  }

  for (const Field &row : Message(object.payload).repeated_field(tile::rows)) {
    if (row.type != WireType::length_delimited) {
      throw std::runtime_error("iwork: malformed tile row");
    }
    const Message info(row.bytes);

    const std::uint32_t index =
        first_row + static_cast<std::uint32_t>(
                        info.number_field(tile_row::index).value_or(0));
    const std::string_view storage =
        info.bytes_field(tile_row::storage).value_or(std::string_view());
    const std::string_view offsets =
        info.bytes_field(tile_row::offsets).value_or(std::string_view());

    // one `std::int16_t` per column, `-1` where the row holds no cell; a cell
    // runs to the next column that has one
    const std::size_t columns = offsets.size() / 2;
    std::vector<std::pair<std::uint32_t, std::size_t>> starts;
    for (std::size_t column = 0; column < columns; ++column) {
      const auto offset = static_cast<std::int16_t>(
          util::byte::from_little_endian<std::uint16_t>(
              offsets.substr(column * 2), 2));
      if (offset < 0) {
        continue;
      }
      if (static_cast<std::size_t>(offset) > storage.size()) {
        throw std::runtime_error("iwork: cell runs past its row storage");
      }
      starts.emplace_back(static_cast<std::uint32_t>(column),
                          static_cast<std::size_t>(offset));
    }

    for (std::size_t i = 0; i < starts.size(); ++i) {
      const auto [column, begin] = starts[i];
      const std::size_t end =
          i + 1 < starts.size() ? starts[i + 1].second : storage.size();
      if (end < begin) {
        throw std::runtime_error("iwork: cell offsets are out of order");
      }
      TableModel::Cell cell = read_cell(storage.substr(begin, end - begin),
                                        index, column, strings, rich_texts);
      if (cell.value_type != ValueType::unknown) {
        // a tile list may name one tile any number of times, so a cell is
        // spent as it is produced rather than once the model is complete
        budget.spend_element();
        budget.spend_text(cell.text.size());
        out.push_back(std::move(cell));
      }
    }
  }
}

} // namespace

} // namespace odr::internal::iwork

namespace odr::internal {

std::string iwork::decimal128_to_string(const std::string_view bytes) {
  if (bytes.size() < 16) {
    throw std::runtime_error("iwork: decimal128 is cut off");
  }

  const std::uint64_t low = read_uint64(bytes, 0);
  const std::uint64_t high = read_uint64(bytes, 8);

  const bool negative = (high >> 63) != 0;
  std::int32_t exponent = 0;
  std::array<std::uint32_t, 4> coefficient{};

  if (((high >> 61) & 0x3) == 0x3) {
    // the form with a coefficient above 10^34, which IEEE 754 leaves
    // non-canonical and reads as zero
    exponent = static_cast<std::int32_t>((high >> 47) & 0x3fff);
  } else {
    exponent = static_cast<std::int32_t>((high >> 49) & 0x3fff);
    const std::uint64_t significand_high = high & ((1ULL << 49) - 1);
    coefficient = {static_cast<std::uint32_t>(low),
                   static_cast<std::uint32_t>(low >> 32),
                   static_cast<std::uint32_t>(significand_high),
                   static_cast<std::uint32_t>(significand_high >> 32)};
  }
  exponent -= decimal128_bias;

  std::string digits;
  while (!is_zero(coefficient)) {
    digits.push_back(static_cast<char>('0' + divide_by_ten(coefficient)));
  }
  if (digits.empty()) {
    return "0";
  }

  // Numbers stores more precision than it shows — 0.075 arrives as
  // 750000000000000e-16 — so the zeros that would only pad the fraction go
  std::size_t significant = 0;
  while (exponent < 0 && significant < digits.size() &&
         digits[significant] == '0') {
    ++significant;
    ++exponent;
  }
  digits.erase(0, significant);
  std::ranges::reverse(digits);

  std::string result;
  if (exponent >= 0) {
    result = digits + std::string(static_cast<std::size_t>(exponent), '0');
  } else {
    const auto fraction = static_cast<std::size_t>(-exponent);
    if (fraction >= digits.size()) {
      result = "0." + std::string(fraction - digits.size(), '0') + digits;
    } else {
      result = digits.substr(0, digits.size() - fraction) + "." +
               digits.substr(digits.size() - fraction);
    }
  }

  return negative ? "-" + result : result;
}

std::string iwork::date_to_string(const double seconds) {
  // an instant is cast to `std::int64_t` below, which is undefined for a value
  // the type cannot hold, and no `std::chrono::year` can name one this far out
  // anyway — so a value beyond the calendar is read as no date at all
  if (!std::isfinite(seconds) || std::abs(seconds) > max_instant) {
    return {};
  }

  // days since the epoch, and the second within the day, with a negative
  // instant flooring rather than truncating toward zero
  const auto total = static_cast<std::int64_t>(std::floor(seconds));
  auto days = static_cast<std::int64_t>(std::floor(total / 86400.0));
  auto rest = static_cast<std::int32_t>(total - days * 86400);

  // Apple counts from 2001-01-01, which is 11323 days after 1970-01-01
  days += 11323;

  const std::chrono::year_month_day date{
      std::chrono::sys_days(std::chrono::days(days))};

  const auto pad = [](const std::int32_t value, const std::size_t width) {
    std::string digits = std::to_string(value);
    return digits.size() >= width
               ? digits
               : std::string(width - digits.size(), '0') + digits;
  };

  return pad(static_cast<std::int32_t>(date.year()), 4) + "-" +
         pad(static_cast<std::int32_t>(
                 static_cast<std::uint32_t>(date.month())),
             2) +
         "-" +
         pad(static_cast<std::int32_t>(static_cast<std::uint32_t>(date.day())),
             2) +
         "T" + pad(rest / 3600, 2) + ":" + pad(rest / 60 % 60, 2) + ":" +
         pad(rest % 60, 2) + "Z";
}

std::string iwork::duration_to_string(const double seconds) {
  if (!std::isfinite(seconds) || std::abs(seconds) > max_instant) {
    return {};
  }

  const bool negative = seconds < 0;
  auto rest = static_cast<std::int64_t>(std::llround(std::abs(seconds)));

  constexpr std::array<std::pair<std::int64_t, char>, 4> units{
      {{86400, 'd'}, {3600, 'h'}, {60, 'm'}, {1, 's'}}};

  std::string result;
  for (const auto &[size, suffix] : units) {
    const std::int64_t count = rest / size;
    rest %= size;
    if (count == 0) {
      continue;
    }
    if (!result.empty()) {
      result += ' ';
    }
    result += std::to_string(count) + suffix;
  }
  if (result.empty()) {
    result = "0s";
  }

  return negative ? "-" + result : result;
}

iwork::TableModel iwork::read_table(Package &package, Budget &budget,
                                    const std::uint64_t identifier) {
  TableModel result;

  // a drawable we have not mapped is a shape Apple ships and we have not seen,
  // so it costs the table it names rather than the file it sits in
  const Object &info_object = package.object(identifier);
  if (info_object.type != archive_type::table_info) {
    return result;
  }
  const std::optional<std::uint64_t> model_identifier =
      reference_identifier(Message(info_object.payload), table_info::model);
  if (!model_identifier.has_value()) {
    return result;
  }

  const Object &model_object = package.object(*model_identifier);
  if (model_object.type != archive_type::table_model) {
    return result;
  }
  const Message model(model_object.payload);

  result.name = std::string(
      model.bytes_field(table_model::name).value_or(std::string_view()));
  result.rows = static_cast<std::uint32_t>(
      model.number_field(table_model::rows).value_or(0));
  result.columns = static_cast<std::uint32_t>(
      model.number_field(table_model::columns).value_or(0));

  const std::optional<std::string_view> store =
      model.bytes_field(table_model::data_store);
  if (!store.has_value()) {
    return result;
  }
  const Message data_store(*store);

  const std::unordered_map<std::uint64_t, Message> strings = read_data_list(
      package, reference_identifier(data_store, data_store::string_list));
  const std::unordered_map<std::uint64_t, Message> rich_texts = read_data_list(
      package, reference_identifier(data_store, data_store::rich_text_list));

  const std::optional<std::string_view> tiles =
      data_store.bytes_field(data_store::tiles);
  if (!tiles.has_value()) {
    return result;
  }
  const Message tile_storage(*tiles);
  const auto rows_per_tile = static_cast<std::uint32_t>(
      tile_storage.number_field(tile_storage::rows_per_tile).value_or(0));

  for (const Field &entry : tile_storage.repeated_field(tile_storage::tiles)) {
    if (entry.type != WireType::length_delimited) {
      throw std::runtime_error("iwork: malformed tile list");
    }
    const Message tile_entry(entry.bytes);
    const auto index = static_cast<std::uint32_t>(
        tile_entry.number_field(tile_storage_entry::index).value_or(0));
    const std::optional<std::uint64_t> tile_identifier =
        reference_identifier(tile_entry, tile_storage_entry::tile);
    if (!tile_identifier.has_value()) {
      continue;
    }
    read_tile(package, budget, *tile_identifier, index * rows_per_tile, strings,
              rich_texts, result.cells);
  }

  return result;
}

const iwork::TableModel &
iwork::TableCache::table(Package &package, Budget &budget,
                         const std::uint64_t identifier) {
  const auto it = m_tables.find(identifier);
  if (it != m_tables.end()) {
    return it->second;
  }
  return m_tables.emplace(identifier, read_table(package, budget, identifier))
      .first->second;
}

} // namespace odr::internal
