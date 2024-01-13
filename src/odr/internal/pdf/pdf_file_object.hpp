#ifndef ODR_INTERNAL_PDF_FILE_OBJECT_HPP
#define ODR_INTERNAL_PDF_FILE_OBJECT_HPP

#include <odr/internal/pdf/pdf_object.hpp>

#include <map>
#include <optional>

namespace odr::internal::pdf {

struct Header {};

struct IndirectObject {
  ObjectReference reference;
  Object object;
  bool has_stream{false};
  std::optional<std::uint32_t> stream_position;
};

struct Trailer {
  std::uint32_t size;

  Dictionary dictionary;

  const ObjectReference &root_reference() const;
};

struct Xref {
  struct Entry {
    std::uint32_t position{};
    bool in_use{};
  };
  using Table = std::map<ObjectReference, Entry>;

  Table table;

  void append(const Xref &xref);
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

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_FILE_OBJECT_HPP
