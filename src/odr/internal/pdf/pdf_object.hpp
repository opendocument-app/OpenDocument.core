#ifndef ODR_INTERNAL_PDF_OBJECT_HPP
#define ODR_INTERNAL_PDF_OBJECT_HPP

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

  StandardString(std::string _string) : string{std::move(_string)} {}

  void to_stream(std::ostream &) const;
  std::string to_string() const;
  friend std::ostream &operator<<(std::ostream &, const StandardString &);
};

struct HexString {
  std::string string;

  HexString(std::string _string) : string{std::move(_string)} {}

  void to_stream(std::ostream &) const;
  std::string to_string() const;
  friend std::ostream &operator<<(std::ostream &, const HexString &);
};

struct Name {
  std::string string;

  Name(std::string _string) : string{std::move(_string)} {}

  void to_stream(std::ostream &) const;
  std::string to_string() const;
  friend std::ostream &operator<<(std::ostream &, const Name &);
};

struct ObjectReference {
  std::uint64_t id{};
  std::uint64_t gen{};

  ObjectReference() = default;
  ObjectReference(std::uint64_t _id, std::uint64_t _gen) : id{_id}, gen{_gen} {}

  [[nodiscard]] bool operator<(const ObjectReference &rhs) const;

  [[nodiscard]] std::size_t hash() const noexcept;

  void to_stream(std::ostream &) const;
  std::string to_string() const;
  friend std::ostream &operator<<(std::ostream &, const ObjectReference &);
};

class Array;
class Dictionary;

class Object {
public:
  using Holder = std::any;

  Object() = default;
  Object(Boolean boolean) : m_holder{boolean} {}
  Object(Integer integer) : m_holder{integer} {}
  Object(Real real) : m_holder{real} {}
  Object(StandardString string) : m_holder{std::move(string)} {}
  Object(HexString string) : m_holder{std::move(string)} {}
  Object(Name name) : m_holder{std::move(name)} {}
  Object(Array);
  Object(Dictionary);
  Object(ObjectReference reference) : m_holder{std::move(reference)} {}

  Holder &holder() { return m_holder; }
  const Holder &holder() const { return m_holder; }

  bool is_null() const { return !m_holder.has_value(); }
  bool is_bool() const { return is<Boolean>(); }
  bool is_integer() const { return is<Integer>(); }
  bool is_real() const { return is<Real>() || is_integer(); }
  bool is_standard_string() const { return is<StandardString>(); }
  bool is_hex_string() const { return is<HexString>(); }
  bool is_name() const { return is<Name>(); }
  bool is_string() const {
    return is_standard_string() || is_hex_string() || is_name();
  }
  bool is_array() const { return is<Array>(); }
  bool is_dictionary() const { return is<Dictionary>(); }
  bool is_reference() const { return is<ObjectReference>(); }

  Boolean as_bool() const { return as<Boolean>(); }
  Integer as_integer() const { return as<Integer>(); }
  Real as_real() const { return is<Real>() ? as<Real>() : as_integer(); }
  const std::string &as_standard_string() const {
    return as<const StandardString &>().string;
  }
  const std::string &as_hex_string() const {
    return as<const HexString &>().string;
  }
  const std::string &as_name() const { return as<const Name &>().string; }
  const std::string &as_string() const;
  const Array &as_array() const & { return as<const Array &>(); }
  const Dictionary &as_dictionary() const & { return as<const Dictionary &>(); }
  const ObjectReference &as_reference() const {
    return as<const ObjectReference &>();
  }

  Array &as_array() & { return as<Array &>(); }
  Dictionary &as_dictionary() & { return as<Dictionary &>(); }

  Array &&as_array() && { return std::move(*this).as<Array &&>(); }
  Dictionary &&as_dictionary() && {
    return std::move(*this).as<Dictionary &&>();
  }

  void to_stream(std::ostream &) const;
  std::string to_string() const;
  friend std::ostream &operator<<(std::ostream &, const Object &);

private:
  Holder m_holder;

  template <typename T> bool is() const { return m_holder.type() == typeid(T); }
  template <typename T> T as() const & { return std::any_cast<T>(m_holder); }
  template <typename T> T as() & { return std::any_cast<T>(m_holder); }
  template <typename T> T as() && {
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

  Holder &holder() { return m_holder; }
  const Holder &holder() const { return m_holder; }

  std::size_t size() const { return m_holder.size(); }
  Holder::iterator begin() { return m_holder.begin(); }
  Holder::iterator end() { return m_holder.end(); }
  Holder::const_iterator begin() const { return m_holder.cbegin(); }
  Holder::const_iterator end() const { return m_holder.cend(); }

  Object &operator[](std::size_t i) { return m_holder.at(i); }
  const Object &operator[](std::size_t i) const { return m_holder.at(i); }

  void to_stream(std::ostream &) const;
  std::string to_string() const;
  friend std::ostream &operator<<(std::ostream &, const Array &);

private:
  Holder m_holder;
};

class Dictionary {
public:
  using Holder = std::map<std::string, Object>;

  Dictionary() = default;
  explicit Dictionary(Holder holder) : m_holder{std::move(holder)} {}

  Holder &holder() { return m_holder; }
  const Holder &holder() const { return m_holder; }

  std::size_t size() const { return m_holder.size(); }
  Holder::iterator begin() { return m_holder.begin(); }
  Holder::iterator end() { return m_holder.end(); }
  Holder::const_iterator begin() const { return m_holder.cbegin(); }
  Holder::const_iterator end() const { return m_holder.cend(); }

  Object &operator[](const std::string &name) { return m_holder[name]; }
  const Object &operator[](const std::string &name) const {
    return m_holder.at(name);
  }

  bool has_key(const std::string &name) const {
    return m_holder.find(name) != std::end(m_holder);
  }

  void to_stream(std::ostream &) const;
  std::string to_string() const;
  friend std::ostream &operator<<(std::ostream &, const Dictionary &);

private:
  Holder m_holder;
};

} // namespace odr::internal::pdf

template <> struct std::hash<odr::internal::pdf::ObjectReference> {
  std::size_t operator()(const odr::internal::pdf::ObjectReference &k) const;
};

#endif // ODR_INTERNAL_PDF_OBJECT_HPP
