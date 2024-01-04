#include <odr/internal/pdf/pdf_file_parser.hpp>

#include <odr/internal/pdf/pdf_file_object.hpp>
#include <odr/internal/util/string_util.hpp>

#include <iostream>
#include <stdexcept>

namespace odr::internal::pdf {

FileParser::FileParser(std::istream &in) : m_parser(in) {}

std::istream &FileParser::in() const { return m_parser.in(); }

std::streambuf &FileParser::sb() const { return m_parser.sb(); }

const ObjectParser &FileParser::parser() const { return m_parser; }

IndirectObject FileParser::read_indirect_object() const {
  IndirectObject result;

  result.reference.first = m_parser.read_unsigned_integer();
  m_parser.skip_whitespace();
  result.reference.second = m_parser.read_unsigned_integer();
  m_parser.skip_whitespace();
  if (std::string line = m_parser.read_line(); line != "obj") {
    throw std::runtime_error("expected obj");
  }

  result.object = m_parser.read_object();
  m_parser.skip_line();

  auto next = m_parser.read_line();

  if (next == "endobj") {
    return result;
  }
  if (next == "stream") {
    result.has_stream = true;
    result.stream_position = in().tellg();
    std::string stream;

    // TODO improve poor solution
    while (true) {
      std::string line = m_parser.read_line(true);
      if (line == "endstream\n") {
        stream.pop_back();
        break;
      }
      stream += line;
    }

    result.stream = std::move(stream);

    if (std::string line = m_parser.read_line(); line != "endobj") {
      throw std::runtime_error("expected endobj");
    }

    return result;
  }

  throw std::runtime_error("expected stream");
}

Trailer FileParser::read_trailer() const {
  if (std::string line = m_parser.read_line(); line != "trailer") {
    throw std::runtime_error("expected trailer");
  }

  Trailer result;

  result.dictionary = m_parser.read_dictionary();
  result.size = result.dictionary["Size"].as_integer();
  result.root_reference = result.dictionary["Root"].as_reference();
  result.info_reference = result.dictionary["Info"].as_reference();

  m_parser.skip_line();

  return result;
}

Xref FileParser::read_xref() const {
  if (std::string line = m_parser.read_line(); line != "xref") {
    throw std::runtime_error("expected xref");
  }

  Xref result;

  while (true) {
    if (!m_parser.peek_number()) {
      return result;
    }

    std::uint32_t first_id = m_parser.read_integer();
    m_parser.skip_whitespace();
    std::uint32_t entry_count = m_parser.read_integer();
    m_parser.skip_line();

    for (std::uint32_t i = 0; i < entry_count; ++i) {
      Xref::Entry entry;

      entry.position = m_parser.read_unsigned_integer();
      m_parser.skip_whitespace();
      entry.generation = m_parser.read_unsigned_integer();
      m_parser.skip_whitespace();
      entry.in_use = m_parser.read_line().at(0) == 'n';

      result.table.emplace(first_id + i, std::move(entry));
    }
  }
}

StartXref FileParser::read_start_xref() const {
  if (std::string line = m_parser.read_line(); line != "startxref") {
    throw std::runtime_error("expected startxref");
  }

  StartXref result;

  result.start = m_parser.read_unsigned_integer();
  m_parser.skip_line();

  return result;
}

void FileParser::read_header() const {
  std::string header1 = m_parser.read_line();
  std::string header2 = m_parser.read_line();

  if (!util::string::starts_with(header1, "%PDF-")) {
    throw std::runtime_error("illegal header");
  }
}

Entry FileParser::read_entry() const {
  m_parser.skip_whitespace();
  std::uint32_t position = in().tellg();
  std::string entry_header = m_parser.read_line();
  in().seekg(position);

  if (util::string::ends_with(entry_header, "obj")) {
    return {read_indirect_object(), position};
  }
  if (entry_header == "xref") {
    return {read_xref(), position};
  }
  if (entry_header == "trailer") {
    return {read_trailer(), position};
  }
  if (entry_header == "startxref") {
    return {read_start_xref(), position};
  }
  if (entry_header == "%PDF-") {
    read_header();
    return {Header{}, position};
  }
  if (entry_header == "%%EOF") {
    return {Eof{}, position};
  }

  throw std::runtime_error("unknown entry");
}

void FileParser::seek_start_xref(std::uint32_t margin) const {
  in().seekg(0, std::ios::end);
  std::int64_t size = in().tellg();
  in().seekg(std::min(0l, size - margin), std::ios::beg);

  while (!m_parser.in().eof()) {
    std::uint32_t position = m_parser.in().tellg();
    std::string line = m_parser.read_line();
    if (line == "startxref") {
      m_parser.in().seekg(position);
      return;
    }
  }

  throw std::runtime_error("unexpected stream exhaust");
}

} // namespace odr::internal::pdf
