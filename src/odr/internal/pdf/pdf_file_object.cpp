#include <odr/internal/pdf/pdf_file_object.hpp>

#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <stdexcept>

namespace odr::internal {

namespace {

/// Cross-reference stream entry type (ISO 32000-1 7.5.8.3, Table 18, field 1).
/// Other values shall be treated as references to the null object (ignored).
enum class XrefStreamType : std::uint64_t {
  free = 0,
  uncompressed = 1,
  compressed = 2,
};

} // namespace

pdf::Xref pdf::parse_xref_stream_table(
    const std::string &data, const std::array<std::uint32_t, 3> &field_widths,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> &subsections) {
  const std::size_t entry_size = static_cast<std::size_t>(field_widths[0]) +
                                 field_widths[1] + field_widths[2];
  std::size_t position = 0;

  const auto read_field = [&](const std::uint32_t width,
                              const std::uint64_t default_value) {
    if (width == 0) {
      return default_value;
    }
    std::uint64_t value = 0;
    for (std::uint32_t i = 0; i < width; ++i) {
      value = (value << 8) | static_cast<unsigned char>(data[position++]);
    }
    return value;
  };

  Xref result;

  for (const auto &[first_id, count] : subsections) {
    for (std::uint32_t i = 0; i < count; ++i) {
      if (position + entry_size > data.size()) {
        throw std::runtime_error("cross-reference stream data exhausted");
      }

      const auto type = static_cast<XrefStreamType>(
          read_field(field_widths[0],
                     static_cast<std::uint64_t>(XrefStreamType::uncompressed)));
      const std::uint64_t second = read_field(field_widths[1], 0);
      const std::uint64_t third = read_field(field_widths[2], 0);
      const std::uint32_t id = first_id + i;

      switch (type) {
      case XrefStreamType::free:
        result.table.emplace(
            ObjectReference(id, third),
            Xref::FreeEntry{static_cast<std::uint32_t>(second),
                            static_cast<std::uint32_t>(third)});
        break;
      case XrefStreamType::uncompressed:
        result.table.emplace(
            ObjectReference(id, third),
            Xref::UsedEntry{static_cast<std::uint32_t>(second)});
        break;
      case XrefStreamType::compressed:
        result.table.emplace(
            ObjectReference(id, 0),
            Xref::CompressedEntry{static_cast<std::uint32_t>(second),
                                  static_cast<std::uint32_t>(third)});
        break;
      }
    }
  }

  return result;
}

pdf::ObjectStream pdf::parse_object_stream(std::istream &in,
                                           const std::uint32_t n,
                                           const std::uint32_t first) {
  // Read the header of `n` (id, offset) pairs, recording the absolute payload
  // position of each member.
  std::vector<std::pair<std::uint64_t, std::uint32_t>> offsets;
  offsets.reserve(n);
  {
    ObjectParser parser(in);
    for (std::uint32_t i = 0; i < n; ++i) {
      parser.skip_whitespace();
      const std::uint64_t id = parser.read_unsigned_integer();
      parser.skip_whitespace();
      const std::uint64_t offset = parser.read_unsigned_integer();
      offsets.emplace_back(id, first + static_cast<std::uint32_t>(offset));
    }
  }

  // Parse each member object (a bare value) at its position.
  ObjectStream members;
  members.reserve(n);
  for (const auto &[id, position] : offsets) {
    in.clear();
    in.seekg(position);
    ObjectParser parser(in);
    parser.skip_whitespace();
    members.push_back({id, parser.read_object()});
  }

  return members;
}

} // namespace odr::internal

namespace odr::internal::pdf {

const ObjectReference &Trailer::root_reference() const {
  return dictionary["Root"].as_reference();
}

void Xref::append(const Xref &xref) {
  table.insert(std::begin(xref.table), std::end(xref.table));
}

void Xref::merge_hybrid(const Xref &xref_stream) {
  for (const auto &[reference, entry] : xref_stream.table) {
    bool absent_or_free = true;
    for (auto it = table.lower_bound(ObjectReference(reference.id, 0));
         it != std::end(table) && it->first.id == reference.id; ++it) {
      if (!it->second.is_free()) {
        absent_or_free = false;
        break;
      }
    }
    if (absent_or_free) {
      table.insert_or_assign(reference, entry);
    }
  }
}

} // namespace odr::internal::pdf
