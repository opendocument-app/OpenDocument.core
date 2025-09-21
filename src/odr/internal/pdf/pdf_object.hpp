#pragma once

#include <any>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace odr::internal::pdf {

using UnsignedInteger = std::uint64_t;
using Integer = std::int64_t;
using Real = double;
using Boolean = bool;

struct StandardString {
  std::string string;

  explicit StandardString(std::string _string) : string{std::move(_string)} {}

  void to_stream(std::ostream &) const;
  [[nodiscard]] std::string to_string() const;
};

struct HexString {
  std::string string;

  explicit HexString(std::string _string) : string{std::move(_string)} {}

  void to_stream(std::ostream &) const;
  [[nodiscard]] std::string to_string() const;
};

struct Name {
  std::string string;

  explicit Name(std::string _string) : string{std::move(_string)} {}

  void to_stream(std::ostream &) const;
  [[nodiscard]] std::string to_string() const;
};

struct ObjectReference {
  std::uint64_t id{};
  std::uint64_t gen{};

  ObjectReference() = default;
  ObjectReference(const std::uint64_t _id, const std::uint64_t _gen)
      : id{_id}, gen{_gen} {}

  [[nodiscard]] bool operator<(const ObjectReference &rhs) const;

  [[nodiscard]] std::size_t hash() const noexcept;

  void to_stream(std::ostream &) const;
  [[nodiscard]] std::string to_string() const;
};

class Array;
class Dictionary;

class Object {
public:
  using Holder = std::any;

  Object() = default;
  explicit Object(Boolean boolean) : m_holder{boolean} {}
  explicit Object(Integer integer) : m_holder{integer} {}
  explicit Object(Real real) : m_holder{real} {}
  explicit Object(StandardString string) : m_holder{std::move(string)} {}
  explicit Object(HexString string) : m_holder{std::move(string)} {}
  explicit Object(Name name) : m_holder{std::move(name)} {}
  explicit Object(Array);
  explicit Object(Dictionary);
  explicit Object(ObjectReference reference) : m_holder{reference} {}

  [[nodiscard]] Holder &holder() { return m_holder; }
  [[nodiscard]] const Holder &holder() const { return m_holder; }

  [[nodiscard]] bool is_null() const { return !m_holder.has_value(); }
  [[nodiscard]] bool is_bool() const { return is<Boolean>(); }
  [[nodiscard]] bool is_integer() const { return is<Integer>(); }
  [[nodiscard]] bool is_real() const { return is<Real>() || is_integer(); }
  [[nodiscard]] bool is_standard_string() const { return is<StandardString>(); }
  [[nodiscard]] bool is_hex_string() const { return is<HexString>(); }
  [[nodiscard]] bool is_name() const { return is<Name>(); }
  [[nodiscard]] bool is_string() const {
    return is_standard_string() || is_hex_string() || is_name();
  }
  [[nodiscard]] bool is_array() const { return is<Array>(); }
  [[nodiscard]] bool is_dictionary() const { return is<Dictionary>(); }
  [[nodiscard]] bool is_reference() const { return is<ObjectReference>(); }

  [[nodiscard]] Boolean as_bool() const { return as<Boolean>(); }
  [[nodiscard]] Integer as_integer() const { return as<Integer>(); }
  [[nodiscard]] Real as_real() const {
    return is<Real>() ? as<Real>() : static_cast<Real>(as_integer());
  }
  [[nodiscard]] const std::string &as_standard_string() const {
    return as<const StandardString &>().string;
  }
  [[nodiscard]] const std::string &as_hex_string() const {
    return as<const HexString &>().string;
  }
  [[nodiscard]] const std::string &as_name() const {
    return as<const Name &>().string;
  }
  [[nodiscard]] const std::string &as_string() const;
  [[nodiscard]] const Array &as_array() const & { return as<const Array &>(); }
  [[nodiscard]] const Dictionary &as_dictionary() const & {
    return as<const Dictionary &>();
  }
  [[nodiscard]] const ObjectReference &as_reference() const {
    return as<const ObjectReference &>();
  }

  Array &as_array() & { return as<Array &>(); }
  Dictionary &as_dictionary() & { return as<Dictionary &>(); }

  Array &&as_array() && { return std::move(*this).as<Array &&>(); }
  Dictionary &&as_dictionary() && {
    return std::move(*this).as<Dictionary &&>();
  }

  void to_stream(std::ostream &) const;
  [[nodiscard]] std::string to_string() const;

private:
  Holder m_holder;

  template <typename T> [[nodiscard]] bool is() const {
    return m_holder.type() == typeid(T);
  }
  template <typename T> [[nodiscard]] T as() const & {
    return std::any_cast<T>(m_holder);
  }
  template <typename T> [[nodiscard]] T as() & {
    return std::any_cast<T>(m_holder);
  }
  template <typename T> [[nodiscard]] T as() && {
    return std::any_cast<T>(std::move(m_holder));
  }
};

class Array {
public:
  using Holder = std::vector<Object>;

  Array() = default;
  explicit Array(Holder holder) : m_holder{std::move(holder)} {}
  Array(const Array &) = default;
  Array(Array &&) = default;

  Array &operator=(const Array &) = default;
  Array &operator=(Array &&) = default;

  [[nodiscard]] Holder &holder() { return m_holder; }
  [[nodiscard]] const Holder &holder() const { return m_holder; }

  [[nodiscard]] std::size_t size() const { return m_holder.size(); }
  [[nodiscard]] Holder::iterator begin() { return m_holder.begin(); }
  [[nodiscard]] Holder::iterator end() { return m_holder.end(); }
  [[nodiscard]] Holder::const_iterator begin() const {
    return m_holder.cbegin();
  }
  [[nodiscard]] Holder::const_iterator end() const { return m_holder.cend(); }

  Object &operator[](const std::size_t i) { return m_holder.at(i); }
  const Object &operator[](const std::size_t i) const { return m_holder.at(i); }

  void to_stream(std::ostream &) const;
  [[nodiscard]] std::string to_string() const;

private:
  Holder m_holder;
};

class Dictionary {
public:
  using Holder = std::map<std::string, Object>;

  Dictionary() = default;
  explicit Dictionary(Holder holder) : m_holder{std::move(holder)} {}

  Holder &holder() { return m_holder; }
  [[nodiscard]] const Holder &holder() const { return m_holder; }

  [[nodiscard]] std::size_t size() const { return m_holder.size(); }
  [[nodiscard]] Holder::iterator begin() { return m_holder.begin(); }
  [[nodiscard]] Holder::iterator end() { return m_holder.end(); }
  [[nodiscard]] Holder::const_iterator begin() const {
    return m_holder.cbegin();
  }
  [[nodiscard]] Holder::const_iterator end() const { return m_holder.cend(); }

  Object &operator[](const std::string &name) { return m_holder[name]; }
  const Object &operator[](const std::string &name) const {
    return m_holder.at(name);
  }

  [[nodiscard]] bool has_key(const std::string &name) const {
    return m_holder.contains(name);
  }

  void to_stream(std::ostream &) const;
  [[nodiscard]] std::string to_string() const;

private:
  Holder m_holder;
};

std::ostream &operator<<(std::ostream &, const StandardString &);
std::ostream &operator<<(std::ostream &, const HexString &);
std::ostream &operator<<(std::ostream &, const Name &);
std::ostream &operator<<(std::ostream &, const ObjectReference &);
std::ostream &operator<<(std::ostream &, const Object &);
std::ostream &operator<<(std::ostream &, const Array &);
std::ostream &operator<<(std::ostream &, const Dictionary &);

} // namespace odr::internal::pdf

template <> struct std::hash<odr::internal::pdf::ObjectReference> {
  std::size_t
  operator()(const odr::internal::pdf::ObjectReference &k) const noexcept;
};
