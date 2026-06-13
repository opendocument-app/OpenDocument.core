#pragma once

#include <odr/internal/pdf/pdf_encryption.hpp>
#include <odr/internal/pdf/pdf_file_object.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>

#include <iosfwd>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace odr {
class Logger;
}

namespace odr::internal::pdf {

struct Document;

/// Resolution/memoization layer on top of the sequential `FileParser`: maps
/// `ObjectReference`s to file positions via the cross-reference table and
/// hands out resolved objects by reference.
///
/// Reads are memoized in `m_objects` / `m_object_streams`. This is intrinsic,
/// not a convenience: a compressed object can only be read by inflating and
/// parsing the whole object stream that holds it, and only this class knows
/// (from the xref) which objects share a stream — so the caller cannot dedupe
/// that work. `read_object` is also reentrant (length resolution, object-stream
/// loading, deep resolution), and the object graph has heavy sharing, so the
/// cache must live here.
///
/// Both caches grow monotonically and are never evicted, which is safe only
/// because a `DocumentParser` is a transient per-render object: build one,
/// produce a `Document`, read its lazy streams, then discard it. Do not hold it
/// long-lived or share it across documents.
class DocumentParser {
public:
  explicit DocumentParser(std::istream &);
  DocumentParser(std::istream &, Logger &logger);

  [[nodiscard]] std::istream &in();
  [[nodiscard]] FileParser &parser();
  [[nodiscard]] const Xref &xref() const;
  [[nodiscard]] Logger &logger() const;

  const IndirectObject &read_object(const ObjectReference &reference);
  std::string read_object_stream(const ObjectReference &reference);
  std::string read_object_stream(const IndirectObject &object);
  /// `read_object_stream` plus the `/Filter` chain (image codecs throw).
  std::string read_decoded_stream(const ObjectReference &reference);
  std::string read_decoded_stream(const IndirectObject &object);

  void resolve_object(Object &object);
  void deep_resolve_object(Object &object);

  Object resolve_object_copy(const Object &object);
  Object deep_resolve_object_copy(const Object &object);

  /// Whether the document declares an `/Encrypt` dictionary (set once the
  /// trailer chain has been walked by `probe_encryption`/`parse_document`).
  [[nodiscard]] bool encrypted() const;
  /// Whether the password supplied so far unlocked the file (true when not
  /// encrypted at all).
  [[nodiscard]] bool authenticated() const;
  /// The authenticated decryptor once a password has unlocked the file, or
  /// `nullptr` if the document is not encrypted / not yet unlocked. `PdfFile`
  /// holds onto this so it can re-open the file without the password — the key
  /// stays sealed inside the `Decryptor`, never exposed as a bare token.
  [[nodiscard]] std::shared_ptr<const Decryptor> decryptor() const;

  /// Walk the trailer chain and set up the decryptor with `password` (the
  /// empty string handles owner-locked files), without parsing the page tree.
  /// Lets `PdfFile` answer `password_encrypted()` cheaply.
  void probe_encryption(const std::string &password = "");

  std::unique_ptr<Document> parse_document(const std::string &password = "");
  /// As `parse_document`, but unlock with an already-authenticated `Decryptor`
  /// (from `decryptor()`) rather than a password — the standard render path, so
  /// the password is never retained. A null `decryptor` parses an unencrypted
  /// document (and, defensively, retries the empty password if encrypted).
  std::unique_ptr<Document>
  parse_document(std::shared_ptr<const Decryptor> decryptor);

private:
  /// Read one cross-reference section (classic table or cross-reference
  /// stream, ISO 32000-1 7.5.4 / 7.5.8) at `position`. The returned
  /// dictionary is the trailer dictionary (the stream dictionary doubles as
  /// one for cross-reference streams).
  std::pair<Xref, Dictionary> read_xref_section(std::uint32_t position);
  const ObjectStream &load_object_stream(std::uint32_t stream_id);

  /// Walk the `startxref` → `Prev` chain, merging sections into `m_xref`, and
  /// return the newest (first-seen) trailer dictionary.
  Dictionary read_trailer_chain();
  /// Parse a page tree into a `Document` once any decryptor is in place.
  std::unique_ptr<Document> build_document(const Dictionary &trailer);
  /// Build the (un-authenticated) decryptor from the trailer `/Encrypt` and
  /// `/ID` (ISO 32000-1 7.6), recording the `/Encrypt` reference for the
  /// self-skip guard. Throws if the trailer is not encrypted.
  Decryptor create_decryptor(const Dictionary &trailer);
  /// Build the decryptor, try `password`, and install it as `m_decryptor`.
  void setup_encryption(const Dictionary &trailer, const std::string &password);
  /// Install an already-authenticated `decryptor` (from another parse of this
  /// file), re-recording the `/Encrypt` reference without reading the dict.
  void adopt_decryptor(const Dictionary &trailer,
                       std::shared_ptr<const Decryptor> decryptor);
  /// Decrypt every string leaf of `object` in place with the owning object's
  /// reference (ISO 32000-1 7.6.2). Used on freshly read indirect objects.
  void decrypt_strings(Object &object, const ObjectReference &reference);

  FileParser m_parser;
  Logger *m_logger{nullptr};
  Xref m_xref;
  std::map<ObjectReference, IndirectObject> m_objects;
  std::map<std::uint32_t, ObjectStream> m_object_streams;
  std::shared_ptr<const Decryptor> m_decryptor;
  std::optional<ObjectReference> m_encrypt_reference;
};

} // namespace odr::internal::pdf
