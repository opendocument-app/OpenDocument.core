#pragma once

#include <string>
#include <vector>

namespace odr::test::pdf {

/// Test-only mini-PDF assembler: collects object snippets and computes the
/// cross-reference offsets and `startxref` itself, so the test source only
/// shows the interesting dictionaries. Object ids are assigned 1..n in
/// insertion order. This must never grow into a writer API.
class PdfFileBuilder {
public:
  /// Add an indirect object; `body` is its value, e.g.
  /// `"<< /Type /Catalog /Pages 2 0 R >>"`.
  PdfFileBuilder &object(std::string body);

  /// Add a stream object; `dictionary_entries` are the dictionary's entries
  /// without the `<< >>` delimiters (`/Length` is computed and added).
  PdfFileBuilder &stream_object(const std::string &dictionary_entries,
                                const std::string &stream_data);

  /// Trailer entries without the `<< >>` delimiters and without `/Size`,
  /// e.g. `"/Root 1 0 R"`.
  PdfFileBuilder &trailer(std::string entries);

  /// Assemble with a classic `xref` table and `trailer`.
  [[nodiscard]] std::string build_classic() const;

  /// Assemble with an uncompressed cross-reference stream (`/W [1 4 2]`,
  /// no `/Filter`) instead of a classic table; the trailer entries go into
  /// the stream dictionary.
  [[nodiscard]] std::string build_xref_stream() const;

private:
  std::vector<std::string> m_objects;
  std::string m_trailer_entries;
};

} // namespace odr::test::pdf
