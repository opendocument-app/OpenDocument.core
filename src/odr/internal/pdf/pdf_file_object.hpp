#pragma once

#include <odr/internal/pdf/pdf_object.hpp>

#include <array>
#include <iosfwd>
#include <map>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

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

  [[nodiscard]] const ObjectReference &root_reference() const;
};

struct Xref {
  /// classic `f` / stream type 0: object on the free list
  struct FreeEntry {
    std::uint32_t next_free_id{};
    std::uint32_t next_generation{};
  };
  /// classic `n` / stream type 1: object stored at a byte offset
  struct UsedEntry {
    std::uint32_t position{};
  };
  /// stream type 2: object stored compressed in an object stream
  struct CompressedEntry {
    std::uint32_t stream_id{};
    std::uint32_t index{};
  };

  class Entry {
  public:
    using Holder = std::variant<FreeEntry, UsedEntry, CompressedEntry>;

    Entry() = default;
    explicit Entry(const FreeEntry &entry) : m_holder{entry} {}
    explicit Entry(const UsedEntry &entry) : m_holder{entry} {}
    explicit Entry(const CompressedEntry &entry) : m_holder{entry} {}

    [[nodiscard]] bool is_free() const {
      return std::holds_alternative<FreeEntry>(m_holder);
    }
    [[nodiscard]] bool is_used() const {
      return std::holds_alternative<UsedEntry>(m_holder);
    }
    [[nodiscard]] bool is_compressed() const {
      return std::holds_alternative<CompressedEntry>(m_holder);
    }

    [[nodiscard]] const FreeEntry &as_free() const {
      return std::get<FreeEntry>(m_holder);
    }
    [[nodiscard]] const UsedEntry &as_used() const {
      return std::get<UsedEntry>(m_holder);
    }
    [[nodiscard]] const CompressedEntry &as_compressed() const {
      return std::get<CompressedEntry>(m_holder);
    }

  private:
    Holder m_holder{FreeEntry{}};
  };

  using Table = std::map<ObjectReference, Entry>;

  Table table;

  /// Merge an older section into this one; existing (newer) entries win.
  void append(const Xref &xref);
  /// Hybrid-reference combine (ISO 32000-1 7.5.8.4): adopt entries from the
  /// `XRefStm` cross-reference stream for ids this (classic) section lacks
  /// or marks free.
  void merge_hybrid(const Xref &xref_stream);
};

/// One member of a decoded object stream: its object id and parsed value.
struct ObjectStreamMember {
  std::uint64_t id{};
  Object object;
};

using ObjectStream = std::vector<ObjectStreamMember>;

struct StartXref {
  std::uint32_t start{};
};

struct Eof {};

class Entry {
public:
  using Holder = std::any;

  Entry(const Header &header, const std::uint32_t position)
      : m_holder{header}, m_position{position} {}
  Entry(IndirectObject object, const std::uint32_t position)
      : m_holder{std::move(object)}, m_position{position} {}
  Entry(Trailer trailer, const std::uint32_t position)
      : m_holder{std::move(trailer)}, m_position{position} {}
  Entry(Xref xref, const std::uint32_t position)
      : m_holder{std::move(xref)}, m_position{position} {}
  Entry(const StartXref &start_xref, const std::uint32_t position)
      : m_holder{start_xref}, m_position{position} {}
  Entry(const Eof &eof, const std::uint32_t position)
      : m_holder{eof}, m_position{position} {}

  Holder &holder() { return m_holder; }
  [[nodiscard]] const Holder &holder() const { return m_holder; }

  [[nodiscard]] std::uint32_t position() const { return m_position; }

  [[nodiscard]] bool is_header() const { return is<Header>(); }
  [[nodiscard]] bool is_object() const { return is<IndirectObject>(); }
  [[nodiscard]] bool is_trailer() const { return is<Trailer>(); }
  [[nodiscard]] bool is_xref() const { return is<Xref>(); }
  [[nodiscard]] bool is_start_xref() const { return is<StartXref>(); }
  [[nodiscard]] bool is_eof() const { return is<Eof>(); }

  [[nodiscard]] const Header &as_header() const { return as<const Header &>(); }
  [[nodiscard]] const IndirectObject &as_object() const {
    return as<const IndirectObject &>();
  }
  [[nodiscard]] const Trailer &as_trailer() const {
    return as<const Trailer &>();
  }
  [[nodiscard]] const Xref &as_xref() const { return as<const Xref &>(); }
  [[nodiscard]] const StartXref &as_start_xref() const {
    return as<const StartXref &>();
  }
  [[nodiscard]] const Eof &as_eof() const { return as<const Eof &>(); }

private:
  Holder m_holder;
  std::uint32_t m_position{};

  template <typename T> [[nodiscard]] bool is() const {
    return m_holder.type() == typeid(T);
  }
  template <typename T> [[nodiscard]] T as() const {
    return std::any_cast<T>(m_holder);
  }
};

} // namespace odr::internal::pdf
