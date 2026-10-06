#include <odr/internal/pdf/pdf_writer.hpp>

#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_file_object.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <algorithm>
#include <istream>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>

namespace odr::internal::pdf {

namespace {

struct Placement {
  std::uint32_t offset{0};
  std::uint32_t gen{0};
};

using Placements = std::map<std::uint64_t, Placement>;

/// A cross-reference subsection (7.5.4): `first count`, then that many entries.
struct Subsection {
  std::uint64_t first{0};
  std::vector<Placement> placements;
};

std::vector<Subsection> group_into_subsections(const Placements &placements) {
  std::vector<Subsection> result;
  for (const auto &[id, placement] : placements) {
    if (result.empty() ||
        result.back().first + result.back().placements.size() != id) {
      result.push_back(Subsection{id, {}});
    }
    result.back().placements.push_back(placement);
  }
  return result;
}

/// Entry table for `/W [1 4 2]` (7.5.8.3); every entry is type 1, in use.
std::string xref_stream_table(const Placements &placements) {
  std::string result;
  for (const auto &[id, placement] : placements) {
    result.push_back(1);
    for (std::int32_t shift = 24; shift >= 0; shift -= 8) {
      result.push_back(static_cast<char>((placement.offset >> shift) & 0xff));
    }
    result.push_back(static_cast<char>((placement.gen >> 8) & 0xff));
    result.push_back(static_cast<char>(placement.gen & 0xff));
  }
  return result;
}

class InputPosition final {
public:
  explicit InputPosition(std::istream &in) : m_in{&in} {
    m_in->clear();
    m_position = m_in->tellg();
    if (m_position == std::streampos{-1}) {
      throw std::ios_base::failure("cannot save PDF input position");
    }
  }

  ~InputPosition() {
    try {
      restore();
    } catch (...) { // NOLINT(bugprone-empty-catch)
      // Preserve the original exception if restoring also fails.
    }
  }

  void restore() {
    if (m_restored) {
      return;
    }
    m_in->clear();
    m_in->seekg(m_position);
    if (!*m_in) {
      throw std::ios_base::failure("cannot restore PDF input position");
    }
    m_restored = true;
  }

private:
  std::istream *m_in{nullptr};
  std::streampos m_position;
  bool m_restored{false};
};

struct SourceExtent {
  std::uint32_t size{0};
  bool ends_with_eol{false};
};

SourceExtent measure(std::istream &in) {
  in.clear();
  in.seekg(0, std::ios::end);
  const auto size = checked_file_index(static_cast<std::streamoff>(in.tellg()));
  if (size == 0) {
    return {0, true};
  }
  in.seekg(-1, std::ios::end);
  const char last = static_cast<char>(in.get());
  if (!in) {
    throw std::ios_base::failure("cannot read PDF source");
  }
  return {size, last == '\n' || last == '\r'};
}

} // namespace

IncrementalWriter::IncrementalWriter(DocumentParser &parser)
    : m_parser{&parser} {
  if (parser.is_recovered()) {
    throw std::runtime_error(
        "cannot append to a file whose cross-reference table was recovered");
  }
  if (parser.is_encrypted()) {
    throw std::runtime_error("cannot append to an encrypted file");
  }
  const std::optional<std::uint32_t> position = parser.start_xref_position();
  const std::optional<DocumentParser::XrefKind> kind = parser.xref_kind();
  if (!position.has_value() || !kind.has_value()) {
    throw std::runtime_error("no cross-reference section to append to");
  }
  m_previous_xref_position = *position;
  m_xref_kind = *kind;
  m_next_id = parser.highest_object_id() + 1;
}

ObjectReference IncrementalWriter::mint_object() {
  checked_file_index(m_next_id, UINT32_MAX - 1);
  return ObjectReference(m_next_id++, 0);
}

void IncrementalWriter::set_object(const ObjectReference &reference,
                                   Object object) {
  set_entry(reference, Entry{std::move(object), std::nullopt});
}

void IncrementalWriter::set_stream_object(const ObjectReference &reference,
                                          Dictionary dictionary,
                                          std::string stream) {
  dictionary["Length"] = Object(Integer{checked_file_index(stream.size())});
  set_entry(reference, Entry{Object(std::move(dictionary)), std::move(stream)});
}

void IncrementalWriter::set_entry(const ObjectReference &reference,
                                  Entry entry) {
  checked_file_index(reference.id, UINT32_MAX - 1);
  checked_file_index(reference.gen, UINT16_MAX);
  if (reference.id == 0) {
    throw std::runtime_error("PDF object zero is reserved");
  }
  const auto existing = m_entries.lower_bound(ObjectReference{reference.id, 0});
  if (existing != m_entries.end() && existing->first.id == reference.id &&
      existing->first.gen != reference.gen) {
    throw std::runtime_error("conflicting PDF object generations");
  }
  m_entries[reference] = std::move(entry);
  m_next_id = std::max(m_next_id, reference.id + 1);
}

Dictionary
IncrementalWriter::build_trailer(const std::uint64_t size,
                                 const std::string_view revision_seed) const {
  const Dictionary &source = m_parser->trailer();

  Dictionary result;
  result["Size"] = Object(Integer{checked_file_index(size)});
  result["Root"] = source.get("Root");
  if (source.has_value("Info")) {
    result["Info"] = source.get("Info");
  }

  // 14.4: `/ID[0]` carries over, `/ID[1]` names this revision — off the
  // update's own bytes, not a clock, so the same update writes the same file.
  const Object &id = source.get("ID");
  if (id.is_array() && id.as_array().size() == 2 &&
      id.as_array()[0].is_string()) {
    Array result_id;
    result_id.holder().emplace_back(HexString{id.as_array()[0].as_string()});
    result_id.holder().emplace_back(
        HexString{crypto::util::md5(revision_seed)});
    result["ID"] = Object(std::move(result_id));
  }

  result["Prev"] = Object(static_cast<Integer>(m_previous_xref_position));
  return result;
}

void IncrementalWriter::write(std::ostream &out) const {
  std::istream &in = m_parser->in();
  InputPosition resume(in);

  const SourceExtent source = measure(in);
  // An object must start on its own line; a `%%EOF` may end the file bare.
  const std::string separator = source.ends_with_eol ? "" : "\n";

  // Only the update is buffered; the source is piped.
  std::string update = separator;
  const auto position = [&] {
    return checked_file_index(std::uint64_t{source.size} + update.size());
  };

  Placements placements;
  for (const auto &[reference, entry] : m_entries) {
    placements[reference.id] =
        Placement{position(), static_cast<std::uint32_t>(reference.gen)};
    update += fmt::format("{} {} obj\n", reference.id, reference.gen);
    update += entry.object.to_string();
    if (entry.stream.has_value()) {
      update += "\nstream\n";
      update += *entry.stream;
      update += "\nendstream";
    }
    update += "\nendobj\n";
  }

  const std::uint32_t xref_position = position();
  std::uint64_t trailer_size = m_parser->highest_object_id() + 1;
  for (const auto &[id, placement] : placements) {
    trailer_size = std::max(trailer_size, id + 1);
  }

  if (m_xref_kind == DocumentParser::XrefKind::table) {
    update += "xref\n";
    for (const Subsection &subsection : group_into_subsections(placements)) {
      update += fmt::format("{} {}\n", subsection.first,
                            subsection.placements.size());
      for (const Placement &placement : subsection.placements) {
        // 7.5.4: exactly 20 bytes, the two-character EOL included
        update +=
            fmt::format("{:010} {:05} n \n", placement.offset, placement.gen);
      }
    }
    update += "trailer\n";
    update += build_trailer(trailer_size, update).to_string();
    update += '\n';
  } else {
    // The stream is an object, so it takes an id and an entry of its own; its
    // dictionary doubles as the trailer (7.5.8).
    const ObjectReference reference(trailer_size, 0);
    placements[reference.id] = Placement{xref_position, 0};
    ++trailer_size;

    const std::string table = xref_stream_table(placements);

    Dictionary dictionary = build_trailer(trailer_size, update);
    dictionary["Type"] = Object(Name{"XRef"});
    Array widths;
    widths.holder().emplace_back(Integer{1});
    widths.holder().emplace_back(Integer{4});
    widths.holder().emplace_back(Integer{2});
    dictionary["W"] = Object(std::move(widths));
    Array index;
    for (const Subsection &subsection : group_into_subsections(placements)) {
      index.holder().emplace_back(static_cast<Integer>(subsection.first));
      index.holder().emplace_back(
          static_cast<Integer>(subsection.placements.size()));
    }
    dictionary["Index"] = Object(std::move(index));
    dictionary["Length"] = Object(static_cast<Integer>(table.size()));

    update += fmt::format("{} {} obj\n", reference.id, reference.gen);
    update += Object(std::move(dictionary)).to_string();
    update += "\nstream\n";
    update += table;
    update += "\nendstream\nendobj\n";
  }

  update += fmt::format("startxref\n{}\n%%EOF\n", xref_position);

  // the finished file has to fit the 32-bit offsets its xref states
  (void)position();
  if (!std::in_range<std::streamsize>(update.size())) {
    throw std::length_error("PDF update is too large");
  }
  in.clear();
  in.seekg(0);
  util::stream::pipe(in, out);
  out.write(update.data(), static_cast<std::streamsize>(update.size()));

  if (!out) {
    throw std::ios_base::failure("cannot write PDF update");
  }
  resume.restore();
}

} // namespace odr::internal::pdf
