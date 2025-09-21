#pragma once

#include <odr/internal/pdf/pdf_file_object.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>

#include <iosfwd>
#include <map>
#include <memory>

namespace odr::internal::pdf {

struct Document;

class DocumentParser {
public:
  explicit DocumentParser(std::istream &);

  [[nodiscard]] std::istream &in() const;
  [[nodiscard]] const FileParser &parser() const;
  [[nodiscard]] const Xref &xref() const;

  const IndirectObject &read_object(const ObjectReference &reference);
  std::string read_object_stream(const ObjectReference &reference);
  std::string read_object_stream(const IndirectObject &object);

  void resolve_object(Object &object);
  void deep_resolve_object(Object &object);

  Object resolve_object_copy(const Object &object);
  Object deep_resolve_object_copy(const Object &object);

  std::unique_ptr<Document> parse_document();

private:
  FileParser m_parser;
  Xref m_xref;
  std::map<ObjectReference, IndirectObject> m_objects;
};

} // namespace odr::internal::pdf
