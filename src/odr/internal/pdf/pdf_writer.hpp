#pragma once

#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_object.hpp>

#include <cstdint>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>

namespace odr::internal::pdf {

/// Appends an incremental update (ISO 32000-1 7.5.6) to the file a
/// `DocumentParser` read. The source is copied, never re-serialized.
class IncrementalWriter final {
public:
  /// @throws std::runtime_error for a file that cannot take one: recovered
  ///         cross-reference table, or encrypted.
  explicit IncrementalWriter(DocumentParser &parser);

  /// An id past every one the file uses.
  [[nodiscard]] ObjectReference mint_object();

  /// Write `object` at `reference`, overriding what the file has there. Keeps
  /// the generation; 7.5.6 raises it only where a freed id is reused.
  void set_object(const ObjectReference &reference, Object object);
  /// `/Length` is computed; the caller supplies any `/Filter` and the matching
  /// pre-encoded bytes.
  void set_stream_object(const ObjectReference &reference,
                         Dictionary dictionary, std::string stream);

  [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }

  /// Legal with nothing set: the result parses identically.
  void write(std::ostream &out) const;

private:
  struct Entry {
    Object object;
    std::optional<std::string> stream;
  };

  /// `/Root`, `/Info` and `/ID` from the source, plus `/Size` and `/Prev`.
  [[nodiscard]] Dictionary build_trailer(std::uint64_t size,
                                         std::string_view revision_seed) const;

  DocumentParser *m_parser{nullptr};
  /// Resolved in the constructor, which rejects a file lacking them.
  std::uint32_t m_previous_xref_position{0};
  DocumentParser::XrefKind m_xref_kind{DocumentParser::XrefKind::table};

  std::map<ObjectReference, Entry> m_entries;
  std::uint64_t m_next_id{0};
};

} // namespace odr::internal::pdf
