#ifndef ODR_INTERNAL_PDF_FILE_PARSER_HPP
#define ODR_INTERNAL_PDF_FILE_PARSER_HPP

#include <odr/internal/pdf/pdf_file_object.hpp>

#include <iostream>
#include <map>
#include <optional>

namespace odr::internal::pdf {

struct Header {};

struct IndirectObject {
  ObjectReference reference;
  Object object;
  bool has_stream{false};
  std::optional<std::uint32_t> stream_position;
  std::optional<std::string> stream;
};

struct Trailer {
  Dictionary dictionary;
};

struct Xref {
  struct Entry {
    std::uint32_t position{};
    std::uint32_t generation{};
    bool in_use{};
  };
  using Table = std::map<std::uint32_t, Entry>;

  Table table;
};

struct StartXref {
  std::uint32_t start{};
};

struct Eof {};

class Entry {
public:
  using Holder = std::any;

  Entry(Header header, std::uint32_t position)
      : m_holder{std::move(header)}, m_position{position} {}
  Entry(IndirectObject object, std::uint32_t position)
      : m_holder{std::move(object)}, m_position{position} {}
  Entry(Trailer trailer, std::uint32_t position)
      : m_holder{std::move(trailer)}, m_position{position} {}
  Entry(Xref xref, std::uint32_t position)
      : m_holder{std::move(xref)}, m_position{position} {}
  Entry(StartXref start_xref, std::uint32_t position)
      : m_holder{std::move(start_xref)}, m_position{position} {}
  Entry(Eof eof, std::uint32_t position)
      : m_holder{std::move(eof)}, m_position{position} {}

  Holder &holder() { return m_holder; }
  const Holder &holder() const { return m_holder; }

  std::uint32_t position() const { return m_position; }

  bool is_header() const { return is<Header>(); }
  bool is_object() const { return is<IndirectObject>(); }
  bool is_trailer() const { return is<Trailer>(); }
  bool is_xref() const { return is<Xref>(); }
  bool is_start_xref() const { return is<StartXref>(); }
  bool is_eof() const { return is<Eof>(); }

  const Header &as_header() const { return as<const Header &>(); }
  const IndirectObject &as_object() const {
    return as<const IndirectObject &>();
  }
  const Trailer &as_trailer() const { return as<const Trailer &>(); }
  const Xref &as_xref() const { return as<const Xref &>(); }
  const StartXref &as_start_xref() const { return as<const StartXref &>(); }
  const Eof &as_eof() const { return as<const Eof &>(); }

private:
  Holder m_holder;
  std::uint32_t m_position{};

  template <typename T> bool is() const { return m_holder.type() == typeid(T); }
  template <typename T> T as() const { return std::any_cast<T>(m_holder); }
};

class PdfObjectParser {
public:
  explicit PdfObjectParser(std::istream &);

  std::istream &in() const;
  std::streambuf &sb() const;

  void skip_whitespace() const;
  void skip_line() const;
  std::string read_line() const;

  bool peek_number() const;
  UnsignedInteger read_unsigned_integer() const;
  Integer read_integer() const;
  Real read_number() const;
  IntegerOrReal read_integer_or_real() const;

  bool peek_name() const;
  void read_name(std::ostream &) const;
  Name read_name() const;

  bool peek_null() const;
  void read_null() const;

  bool peek_boolean() const;
  Boolean read_boolean() const;

  bool peek_string() const;
  void read_string(std::ostream &) const;
  String read_string() const;

  bool peek_array() const;
  Array read_array() const;

  bool peek_dictionary() const;
  Dictionary read_dictionary() const;

  Object read_object() const;

  ObjectReference read_object_reference() const;

private:
  std::istream *m_in;
  std::istream::sentry m_se;
  std::streambuf *m_sb;
};

class PdfFileParser {
public:
  explicit PdfFileParser(std::istream &);

  std::istream &in() const;
  std::streambuf &sb() const;
  const PdfObjectParser &parser() const;

  IndirectObject read_indirect_object() const;
  Trailer read_trailer() const;
  Xref read_xref() const;
  StartXref read_startxref() const;

  void read_header() const;
  Entry read_entry() const;

  void seek_startxref(std::uint32_t margin = 64) const;

private:
  PdfObjectParser m_parser;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_FILE_PARSER_HPP
