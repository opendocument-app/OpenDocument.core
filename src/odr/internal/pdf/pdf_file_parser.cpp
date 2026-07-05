#include <odr/internal/pdf/pdf_file_parser.hpp>

#include <odr/internal/pdf/pdf_file_object.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <stdexcept>

namespace odr::internal::pdf {

FileParser::FileParser(std::istream &in) : m_parser(in) {}

std::istream &FileParser::in() { return m_parser.in(); }

std::streambuf &FileParser::sb() { return m_parser.sb(); }

ObjectParser &FileParser::parser() { return m_parser; }

IndirectObject FileParser::read_indirect_object() {
  IndirectObject result;

  result.reference.id = m_parser.read_unsigned_integer();
  m_parser.skip_whitespace();
  result.reference.gen = m_parser.read_unsigned_integer();
  m_parser.skip_whitespace();
  // `obj` is not necessarily followed by a newline; some producers keep the
  // object on the same line
  m_parser.expect_characters("obj");
  m_parser.skip_whitespace();

  result.object = m_parser.read_object();
  m_parser.skip_whitespace();
  // an indirect object whose body is itself a reference (`5 0 obj 12 0 R
  // endobj`): the body is followed by `endobj`/`stream`, so a digit here can
  // only be the generation of an `n g R`
  m_parser.promote_indirect_reference(result.object);

  // the keyword may carry trailing whitespace (`endobj \n`) or a CR from a
  // CRLF line ending (`stream\r\n`), so compare against the trimmed token
  std::string next = m_parser.read_line();
  util::string::rtrim_inplace(next);

  if (next == "endobj") {
    m_parser.skip_whitespace();
    return result;
  }
  if (next == "stream") {
    result.has_stream = true;
    result.stream_position = in().tellg();

    m_parser.skip_whitespace();
    return result;
  }

  throw std::runtime_error("expected stream");
}

Trailer FileParser::read_trailer() {
  m_parser.expect_characters("trailer");
  m_parser.skip_whitespace();

  Trailer result;

  result.dictionary = m_parser.read_dictionary();
  result.size = result.dictionary["Size"].as_integer();

  m_parser.skip_line();
  m_parser.skip_whitespace();

  return result;
}

Xref FileParser::read_xref() {
  if (const std::string line = m_parser.read_line(); line != "xref") {
    throw std::runtime_error("expected xref");
  }

  Xref result;

  while (true) {
    if (!m_parser.peek_number()) {
      m_parser.skip_whitespace();
      return result;
    }

    const std::uint32_t first_id = m_parser.read_integer();
    m_parser.skip_whitespace();
    const std::uint32_t entry_count = m_parser.read_integer();
    m_parser.skip_line();

    for (std::uint32_t i = 0; i < entry_count; ++i) {
      const std::uint32_t field1 = m_parser.read_unsigned_integer();
      m_parser.skip_whitespace();
      const std::uint32_t field2 = m_parser.read_unsigned_integer();
      m_parser.skip_whitespace();
      const bool in_use = m_parser.read_line().at(0) == 'n';

      const Xref::Entry entry =
          in_use ? Xref::Entry(Xref::UsedEntry{field1})
                 : Xref::Entry(Xref::FreeEntry{field1, field2});
      result.table.emplace(ObjectReference(first_id + i, field2), entry);
    }
  }
}

StartXref FileParser::read_start_xref() {
  if (const std::string line = m_parser.read_line(); line != "startxref") {
    throw std::runtime_error("expected startxref");
  }

  StartXref result;

  result.start = m_parser.read_unsigned_integer();
  m_parser.skip_line();
  m_parser.skip_whitespace();

  return result;
}

std::string FileParser::read_stream(const std::optional<std::uint32_t> size) {
  std::string result;

  if (size.has_value()) {
    result = util::stream::read(in(), *size);

    // The EOL before `endstream` is recommended but not counted in the stream
    // length, and some producers omit it entirely (7.3.8.1) — so skip optional
    // whitespace rather than assuming a whole line precedes the keyword.
    m_parser.skip_whitespace();
    m_parser.expect_characters("endstream");
  } else {
    // Length unknown (recovery path): scan raw bytes until the `endstream`
    // keyword, keeping everything before it. Byte-exact rather than line-based
    // so binary data survives, and the keyword is matched anywhere rather than
    // only on its own line.
    static const std::string keyword = "endstream";
    while (!util::string::ends_with(result, keyword)) {
      result.push_back(m_parser.bumpc());
    }
    result.erase(result.size() - keyword.size());

    // The EOL immediately before the keyword delimits it from the data and is
    // not part of the stream (7.3.8.1); strip one CRLF / LF / CR if present.
    if (!result.empty() && result.back() == '\n') {
      result.pop_back();
    }
    if (!result.empty() && result.back() == '\r') {
      result.pop_back();
    }
  }

  m_parser.skip_whitespace();
  m_parser.expect_characters("endobj");
  m_parser.skip_whitespace();

  return result;
}

ObjectStream FileParser::read_object_stream(const std::uint32_t n,
                                            const std::uint32_t first) {
  // Read the header of `n` (id, offset) pairs, recording the absolute payload
  // position of each member.
  std::vector<std::pair<std::uint64_t, std::uint32_t>> offsets;
  offsets.reserve(n);
  {
    for (std::uint32_t i = 0; i < n; ++i) {
      parser().skip_whitespace();
      const std::uint64_t id = parser().read_unsigned_integer();
      parser().skip_whitespace();
      const std::uint64_t offset = parser().read_unsigned_integer();
      offsets.emplace_back(id, first + static_cast<std::uint32_t>(offset));
    }
  }

  // Parse each member object (a bare value) at its position.
  ObjectStream members;
  members.reserve(n);
  for (const auto &[id, position] : offsets) {
    in().clear();
    in().seekg(position);
    parser().skip_whitespace();
    members.push_back({id, parser().read_object()});
  }

  return members;
}

void FileParser::read_header() {
  const std::string header1 = m_parser.read_line();
  // the second line is an optional binary-marker comment; read past it
  [[maybe_unused]] const std::string header2 = m_parser.read_line();

  if (!util::string::starts_with(header1, "%PDF-")) {
    throw std::runtime_error("illegal header");
  }

  m_parser.skip_whitespace();
}

Entry FileParser::read_entry() {
  std::uint32_t position = in().tellg();
  const std::string entry_header = m_parser.read_line();
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

void FileParser::seek_start_xref(const std::uint32_t margin) {
  in().seekg(0, std::ios::end);
  const std::int64_t size = in().tellg();
  in().seekg(std::max(static_cast<std::int64_t>(0), size - margin),
             std::ios::beg);

  while (!m_parser.in().eof()) {
    const std::uint32_t position = m_parser.in().tellg();
    if (const std::string line = m_parser.read_line(); line == "startxref") {
      m_parser.in().seekg(position);
      return;
    }
  }

  throw std::runtime_error("unexpected stream exhaust");
}

Xref FileParser::read_xref_stream_table(
    const std::array<std::uint32_t, 3> &field_widths,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>> &subsections) {
  /// Cross-reference stream entry type (ISO 32000-1 7.5.8.3, Table 18, field
  /// 1). Other values shall be treated as references to the null object
  /// (ignored).
  enum class XrefStreamType : std::uint64_t {
    free = 0,
    uncompressed = 1,
    compressed = 2,
  };

  const auto read_field = [&](const std::uint32_t width,
                              const std::uint64_t default_value) {
    if (width == 0) {
      return default_value;
    }
    std::uint64_t value = 0;
    for (std::uint32_t i = 0; i < width; ++i) {
      const auto c = parser().bumpc();
      value = (value << 8) | static_cast<std::uint8_t>(c);
    }
    return value;
  };

  Xref result;

  for (const auto &[first_id, count] : subsections) {
    for (std::uint32_t i = 0; i < count; ++i) {
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

namespace {

/// Trim leading and trailing PDF whitespace from `line`, returning the offset
/// of the first non-whitespace byte (so the caller can map back to a file
/// position) and a view of the trimmed content.
std::pair<std::size_t, std::string_view> trim_line(const std::string &line) {
  const std::string_view content =
      util::string::trim_view(line, &ObjectParser::is_whitespace);
  // `content` is a subrange of `line`, so the leading offset is the distance
  // between their data pointers.
  return {static_cast<std::size_t>(content.data() - line.data()), content};
}

/// Recognize an `n g obj` object header at the start of `content` (already
/// trimmed). The dictionary/value may follow on the same line (`12 0 obj<<`),
/// so only the leading `id gen obj` token is required.
std::optional<ObjectReference>
match_object_start(const std::string_view content) {
  util::stream::ViewStream stream(content);
  ObjectParser parser(stream);

  // `peek_unsigned_integer` guards each read so a non-matching line is rejected
  // without `read_unsigned_integer` throwing (the common case while scanning).
  if (!parser.peek_unsigned_integer()) {
    return std::nullopt;
  }
  const UnsignedInteger id = parser.read_unsigned_integer();
  if (!parser.peek_whitespace()) {
    return std::nullopt;
  }
  parser.skip_whitespace();
  if (!parser.peek_unsigned_integer()) {
    return std::nullopt;
  }
  const UnsignedInteger gen = parser.read_unsigned_integer();
  if (!parser.peek_whitespace()) {
    return std::nullopt;
  }
  parser.skip_whitespace();

  // the `obj` keyword must follow; guard against identifiers like `object`
  // that merely start with `obj`
  const std::string rest = parser.read_line();
  const std::string_view tail(rest);
  if (!tail.starts_with("obj")) {
    return std::nullopt;
  }
  if (tail.size() > 3 &&
      (std::isalnum(static_cast<std::uint8_t>(tail[3])) || tail[3] == '.')) {
    return std::nullopt;
  }
  return ObjectReference(id, gen);
}

/// True if `content` (already trimmed) ends with the `stream` keyword on a word
/// boundary. This covers both a bare `stream` line and a compact object that
/// inlines its dictionary and the `stream` token on one line
/// (`N G obj<<...>>stream`). The boundary check rejects `endstream` and
/// identifiers that merely end in `stream`.
bool opens_stream_body(const std::string_view content) {
  constexpr std::string_view keyword = "stream";
  if (!content.ends_with(keyword)) {
    return false;
  }
  const std::size_t begin = content.size() - keyword.size();
  return begin == 0 ||
         !std::isalnum(static_cast<std::uint8_t>(content[begin - 1]));
}

} // namespace

std::pair<Xref, Dictionary> FileParser::recover_xref() {
  ObjectParser &p = parser();
  std::istream &stream = in();
  stream.clear();
  stream.seekg(0, std::ios::end);
  const auto size = static_cast<std::uint32_t>(stream.tellg());

  Xref xref;
  Dictionary trailer;

  stream.seekg(0);
  while (true) {
    stream.clear(); // drop any eofbit set by the previous read_line
    const std::int64_t tell = stream.tellg();
    if (tell < 0 || static_cast<std::uint32_t>(tell) >= size) {
      break;
    }
    const auto position = static_cast<std::uint32_t>(tell);

    const std::string line = p.read_line();
    const auto [lead, content] = trim_line(line);

    if (const std::optional<ObjectReference> ref =
            match_object_start(content)) {
      // last definition of an id wins (operator[] overwrites)
      xref.table[*ref] = Xref::Entry(
          Xref::UsedEntry{static_cast<std::uint32_t>(position + lead)});
      // A compact object may inline its dictionary and the `stream` token on
      // this same line; fall through to skip the body below. Otherwise the
      // header is fully consumed and we advance to the next line.
      if (!opens_stream_body(content)) {
        continue;
      }
    }

    if (opens_stream_body(content)) {
      // Skip the stream body so its (possibly object-shaped) bytes are not
      // mis-scanned. The length is unknown here, so scan past `endstream`.
      stream.clear();
      p.skip_past("endstream");
      continue;
    }

    if (content.starts_with("trailer")) {
      const std::int64_t after = stream.tellg(); // start of the next line
      try {
        stream.clear();
        stream.seekg(static_cast<std::int64_t>(position + lead) +
                     7); // "trailer"
        p.skip_whitespace();
        for (const Dictionary dict = p.read_dictionary();
             const auto &[key, value] : dict) {
          trailer[key] = value; // last trailer wins per key
        }
      } catch (const std::exception &) { // NOLINT(bugprone-empty-catch)
        // ignore a malformed trailer and keep scanning
      }
      stream.clear();
      if (after >= 0) {
        stream.seekg(after);
      }
      continue;
    }
  }

  return {std::move(xref), std::move(trailer)};
}

} // namespace odr::internal::pdf
