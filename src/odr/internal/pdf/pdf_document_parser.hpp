#pragma once

#include <odr/logger.hpp>

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
  /// Takes ownership of the input stream (move-only: the parser is the
  /// top-level handle for reading a PDF, so the stream's lifetime is tied to
  /// it). The constructor walks the trailer chain and, if the file declares an
  /// `/Encrypt` dictionary, builds the `Authenticator`. Pass an already
  /// authenticated `decryptor` (from a prior `authenticate()` / `decryptor()`)
  /// to re-open an encrypted file without the password; otherwise pass
  /// `std::nullopt` to parse an unencrypted file or to call `authenticate()`
  /// later. The logger is borrowed and must outlive the parser.
  explicit DocumentParser(std::unique_ptr<std::istream> in,
                          std::optional<Decryptor> decryptor = std::nullopt,
                          const Logger &logger = Logger::null());

  [[nodiscard]] std::istream &in();
  [[nodiscard]] FileParser &parser();
  [[nodiscard]] const Logger &logger() const;

  [[nodiscard]] const Xref &xref() const;
  [[nodiscard]] const Dictionary &trailer() const;

  /// Whether the file declares an `/Encrypt` dictionary.
  [[nodiscard]] bool is_encrypted() const;
  /// Whether the file is encrypted and a decryptor is installed, so reads can
  /// decrypt. False for an encrypted file that has not been unlocked.
  [[nodiscard]] bool is_authenticated() const;
  /// The authenticator for an encrypted file (validates passwords and produces
  /// a `Decryptor`), or `nullopt` if the file is not encrypted.
  [[nodiscard]] const std::optional<Authenticator> &authenticator() const;
  /// The decryptor once the file is unlocked — supplied at construction or
  /// established by a successful `authenticate()` — or `nullopt` otherwise.
  [[nodiscard]] const std::optional<Decryptor> &decryptor() const;

  /// Try `password` against the authenticator and, on success, install the
  /// resulting decryptor so subsequent reads decrypt. Returns whether the
  /// password was accepted. Throws if the file is already authenticated or is
  /// not encrypted (guard with `is_encrypted()`).
  bool authenticate(const std::string &password);

  /// Parse the page tree into a `Document`. The file must already be readable:
  /// unencrypted, or unlocked via a construction-time decryptor or a successful
  /// `authenticate()`.
  [[nodiscard]] std::unique_ptr<Document> parse_document();

  [[nodiscard]] const IndirectObject &
  read_object(const ObjectReference &reference);
  [[nodiscard]] std::string
  read_object_stream(const ObjectReference &reference);
  [[nodiscard]] std::string read_object_stream(const IndirectObject &object);
  /// `read_object_stream` plus the `/Filter` chain (image codecs throw).
  [[nodiscard]] std::string
  read_decoded_stream(const ObjectReference &reference);
  [[nodiscard]] std::string read_decoded_stream(const IndirectObject &object);

  void resolve_object(Object &object);
  void deep_resolve_object(Object &object);

  [[nodiscard]] Object resolve_object_copy(const Object &object);
  [[nodiscard]] Object deep_resolve_object_copy(const Object &object);

private:
  /// Read one cross-reference section (classic table or cross-reference
  /// stream, ISO 32000-1 7.5.4 / 7.5.8) at `position`. The returned
  /// dictionary is the trailer dictionary (the stream dictionary doubles as
  /// one for cross-reference streams).
  [[nodiscard]] std::pair<Xref, Dictionary>
  read_xref_section(std::uint32_t position);

  /// Walk the `startxref` → `Prev` chain and return the merged cross-reference
  /// table together with the newest (first-seen) trailer dictionary.
  [[nodiscard]] std::pair<Xref, Dictionary> read_trailer_chain();

  /// Last-resort cross-reference recovery for broken files (missing/garbage
  /// `startxref`, wrong offsets, a damaged chain): forward-scan the whole file
  /// for `n g obj` starts, rebuilding `m_xref` (last definition of an id wins)
  /// and collecting `trailer` dictionaries into `m_trailer`. Then object-stream
  /// members are indexed (`index_object_streams`) and, if no `trailer` supplied
  /// a `/Root`, a `/Type /Catalog` object is searched (`recover_root`). Sets
  /// `m_recovered`. Any object cached from the failed attempt is dropped first.
  void recover_xref();
  /// Index the members of every recovered `/Type /ObjStm` object as compressed
  /// cross-reference entries (additive; an existing direct entry wins).
  void index_object_streams();
  /// Search the recovered objects for a `/Type /Catalog` and install it as the
  /// trailer `/Root`.
  void recover_root();

  [[nodiscard]] const ObjectStream &load_object_stream(std::uint32_t stream_id);

  /// Decrypt every string leaf of `object` in place with the owning object's
  /// reference (ISO 32000-1 7.6.2). Used on freshly read indirect objects.
  void decrypt_strings(Object &object, const ObjectReference &reference);

  std::unique_ptr<std::istream> m_stream;
  FileParser m_parser;
  const Logger *m_logger{nullptr};

  Xref m_xref;
  Dictionary m_trailer;
  bool m_recovered{false};

  bool m_is_encrypted{false};
  std::optional<Authenticator> m_authenticator;
  std::optional<Decryptor> m_decryptor;

  std::map<ObjectReference, IndirectObject> m_objects;
  std::map<std::uint32_t, ObjectStream> m_object_streams;
};

} // namespace odr::internal::pdf
