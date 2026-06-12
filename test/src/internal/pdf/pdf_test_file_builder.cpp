#include "pdf_test_file_builder.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>

namespace odr::test::pdf {

PdfFileBuilder &PdfFileBuilder::object(std::string body) {
  m_objects.push_back(std::move(body));
  return *this;
}

PdfFileBuilder &
PdfFileBuilder::stream_object(const std::string &dictionary_entries,
                              const std::string &stream_data) {
  std::string body = "<< " + dictionary_entries + " /Length " +
                     std::to_string(stream_data.size()) + " >>\nstream\n" +
                     stream_data + "\nendstream";
  m_objects.push_back(std::move(body));
  return *this;
}

PdfFileBuilder &PdfFileBuilder::trailer(std::string entries) {
  m_trailer_entries = std::move(entries);
  return *this;
}

namespace {

/// header plus the wrapped objects; `offsets` receives one byte offset per
/// object
std::string assemble_body(const std::vector<std::string> &objects,
                          std::vector<std::uint32_t> &offsets) {
  std::string result = "%PDF-1.7\n";
  for (std::size_t i = 0; i < objects.size(); ++i) {
    offsets.push_back(result.size());
    result += std::to_string(i + 1) + " 0 obj\n" + objects[i] + "\nendobj\n";
  }
  return result;
}

} // namespace

std::string PdfFileBuilder::build_classic() const {
  std::vector<std::uint32_t> offsets;
  std::string result = assemble_body(m_objects, offsets);

  const std::uint32_t xref_position = result.size();
  std::ostringstream xref;
  xref << "xref\n0 " << (m_objects.size() + 1) << "\n";
  xref << "0000000000 65535 f \n";
  for (const std::uint32_t offset : offsets) {
    xref << std::setfill('0') << std::setw(10) << offset << " 00000 n \n";
  }
  result += xref.str();

  result += "trailer\n<< /Size " + std::to_string(m_objects.size() + 1) + " " +
            m_trailer_entries + " >>\n";
  result += "startxref\n" + std::to_string(xref_position) + "\n%%EOF\n";

  return result;
}

std::string PdfFileBuilder::build_xref_stream() const {
  std::vector<std::uint32_t> offsets;
  std::string result = assemble_body(m_objects, offsets);

  const std::uint32_t xref_id = m_objects.size() + 1;
  const std::uint32_t xref_position = result.size();
  offsets.push_back(xref_position);

  // one entry per object plus the free head (id 0) and the cross-reference
  // stream itself; W [1 4 2]: type, big-endian offset, generation
  std::string table;
  const auto entry = [&table](const std::uint8_t type,
                              const std::uint32_t second,
                              const std::uint16_t third) {
    table.push_back(static_cast<char>(type));
    table.push_back(static_cast<char>((second >> 24) & 0xff));
    table.push_back(static_cast<char>((second >> 16) & 0xff));
    table.push_back(static_cast<char>((second >> 8) & 0xff));
    table.push_back(static_cast<char>(second & 0xff));
    table.push_back(static_cast<char>((third >> 8) & 0xff));
    table.push_back(static_cast<char>(third & 0xff));
  };
  entry(0, 0, 0xffff);
  for (const std::uint32_t offset : offsets) {
    entry(1, offset, 0);
  }

  result += std::to_string(xref_id) + " 0 obj\n<< /Type /XRef /Size " +
            std::to_string(xref_id + 1) + " /W [1 4 2] /Length " +
            std::to_string(table.size()) + " " + m_trailer_entries +
            " >>\nstream\n" + table + "\nendstream\nendobj\n";
  result += "startxref\n" + std::to_string(xref_position) + "\n%%EOF\n";

  return result;
}

} // namespace odr::test::pdf
