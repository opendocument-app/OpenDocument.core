#pragma once

#include <odr/internal/pdf/pdf_file_object.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>

#include <iosfwd>
#include <map>
#include <memory>
#include <utility>

namespace odr {
class Logger;
}

namespace odr::internal::pdf {

struct Document;

class DocumentParser {
public:
  explicit DocumentParser(std::istream &);
  DocumentParser(std::istream &, Logger &logger);

  [[nodiscard]] std::istream &in();
  [[nodiscard]] FileParser &parser();
  [[nodiscard]] const Xref &xref() const;

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

  std::unique_ptr<Document> parse_document();

private:
  /// Read one cross-reference section (classic table or cross-reference
  /// stream, ISO 32000-1 7.5.4 / 7.5.8) at `position`. The returned
  /// dictionary is the trailer dictionary (the stream dictionary doubles as
  /// one for cross-reference streams).
  std::pair<Xref, Dictionary> read_xref_section(std::uint32_t position);
  ObjectStream &load_object_stream(std::uint32_t stream_id);

  FileParser m_parser;
  Logger *m_logger;
  Xref m_xref;
  std::map<ObjectReference, IndirectObject> m_objects;
  std::map<std::uint32_t, ObjectStream> m_object_streams;
};

} // namespace odr::internal::pdf
