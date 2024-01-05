#ifndef ODR_INTERNAL_PDF_OBJECT_HPP
#define ODR_INTERNAL_PDF_OBJECT_HPP

#include <any>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace odr::internal::pdf {

using UnsignedInteger = std::uint64_t;
using Integer = std::int64_t;
using Real = double;
using IntegerOrReal = std::variant<Integer, Real>;
using String = std::string;
using Name = std::string;
using Boolean = bool;
using ObjectReference = std::pair<UnsignedInteger, UnsignedInteger>;

class Array;
class Dictionary;

class Object {
public:
  using Holder = std::any;

  Object() = default;
  Object(Boolean boolean) : m_holder{boolean} {}
  Object(Integer integer) : m_holder{integer} {}
  Object(Real real) : m_holder{real} {}
  Object(std::string string) : m_holder{std::move(string)} {}
  Object(Array);
  Object(Dictionary);
  Object(ObjectReference reference) : m_holder{std::move(reference)} {}

  Holder &holder() { return m_holder; }
  const Holder &holder() const { return m_holder; }

  bool is_null() const { return !m_holder.has_value(); }
  bool is_bool() const { return is<Boolean>(); }
  bool is_integer() const { return is<Integer>(); }
  bool is_real() const { return is<Real>() || is_integer(); }
  bool is_string() const { return is<std::string>(); }
  bool is_array() const { return is<Array>(); }
  bool is_dictionary() const { return is<Dictionary>(); }
  bool is_reference() const { return is<ObjectReference>(); }

  Boolean as_bool() const { return as<Boolean>(); }
  Integer as_integer() const { return as<Integer>(); }
  Real as_real() const { return is<Real>() ? as<Real>() : as_integer(); }
  const std::string &as_string() const { return as<const std::string &>(); }
  const Array &as_array() const { return as<const Array &>(); }
  const Dictionary &as_dictionary() const { return as<const Dictionary &>(); }
  const ObjectReference &as_reference() const {
    return as<const ObjectReference &>();
  }

private:
  Holder m_holder;

  template <typename T> bool is() const { return m_holder.type() == typeid(T); }
  template <typename T> T as() const { return std::any_cast<T>(m_holder); }
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
  Holder::const_iterator begin() const { return std::begin(m_holder); }
  Holder::const_iterator end() const { return std::end(m_holder); }

  const Object &operator[](std::size_t i) const { return m_holder.at(i); }

private:
  Holder m_holder;
};

class Dictionary {
public:
  using Holder = std::map<Name, Object>;

  Dictionary() = default;
  explicit Dictionary(Holder holder) : m_holder{std::move(holder)} {}

  Holder &holder() { return m_holder; }
  const Holder &holder() const { return m_holder; }

  std::size_t size() const { return m_holder.size(); }
  Holder::const_iterator begin() const { return std::begin(m_holder); }
  Holder::const_iterator end() const { return std::end(m_holder); }

  const Object &operator[](const Name &name) const { return m_holder.at(name); }

  bool has_key(const Name &name) const {
    return m_holder.find(name) != std::end(m_holder);
  }

private:
  Holder m_holder;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_OBJECT_HPP
