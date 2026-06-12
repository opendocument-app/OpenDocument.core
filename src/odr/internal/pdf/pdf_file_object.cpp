#include <odr/internal/pdf/pdf_file_object.hpp>

#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <stdexcept>

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

Xref parse_xref_stream_table(
    const std::string &data, const std::vector<std::uint32_t> &field_widths,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> &subsections) {
  if (field_widths.size() != 3) {
    throw std::runtime_error(
        "expected three field widths in cross-reference stream /W");
  }

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

      const std::uint64_t type = read_field(field_widths[0], 1);
      const std::uint64_t second = read_field(field_widths[1], 0);
      const std::uint64_t third = read_field(field_widths[2], 0);
      const std::uint32_t id = first_id + i;

      if (type == 0) {
        result.table.emplace(
            ObjectReference(id, third),
            Xref::FreeEntry{static_cast<std::uint32_t>(second),
                            static_cast<std::uint32_t>(third)});
      } else if (type == 1) {
        result.table.emplace(
            ObjectReference(id, third),
            Xref::UsedEntry{static_cast<std::uint32_t>(second)});
      } else if (type == 2) {
        result.table.emplace(
            ObjectReference(id, 0),
            Xref::CompressedEntry{static_cast<std::uint32_t>(second),
                                  static_cast<std::uint32_t>(third)});
      }
    }
  }

  return result;
}

ObjectStream::ObjectStream(std::string data, const std::uint32_t n,
                           const std::uint32_t first)
    : m_in{std::move(data)} {
  ObjectParser parser(m_in);
  m_members.reserve(n);
  for (std::uint32_t i = 0; i < n; ++i) {
    parser.skip_whitespace();
    const std::uint64_t id = parser.read_unsigned_integer();
    parser.skip_whitespace();
    const std::uint64_t offset = parser.read_unsigned_integer();
    m_members.emplace_back(id, first + static_cast<std::uint32_t>(offset));
  }
}

Object ObjectStream::member_object(const std::uint32_t index) {
  if (index >= m_members.size()) {
    throw std::runtime_error("object stream member index out of range");
  }
  m_in.clear();
  m_in.seekg(m_members[index].second);
  ObjectParser parser(m_in);
  parser.skip_whitespace();
  return parser.read_object();
}

} // namespace odr::internal::pdf
