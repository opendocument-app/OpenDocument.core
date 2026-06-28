#pragma once

#include <any>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace odr::internal::pdf {

using UnsignedInteger = std::uint64_t;
using Integer = std::int64_t;
using Real = double;
using Boolean = bool;

struct StandardString final {
  std::string string;

  explicit StandardString(std::string _string) : string{std::move(_string)} {}

  void to_stream(std::ostream &) const;
  [[nodiscard]] std::string to_string() const;
};

struct HexString final {
  std::string string;

  explicit HexString(std::string _string) : string{std::move(_string)} {}

  void to_stream(std::ostream &) const;
  [[nodiscard]] std::string to_string() const;
};

struct Name final {
  std::string string;

  explicit Name(std::string _string) : string{std::move(_string)} {}

  void to_stream(std::ostream &) const;
  [[nodiscard]] std::string to_string() const;
};

struct ObjectReference final {
  std::uint64_t id{};
  std::uint64_t gen{};

  ObjectReference() = default;
  ObjectReference(const std::uint64_t _id, const std::uint64_t _gen)
      : id{_id}, gen{_gen} {}

  [[nodiscard]] bool operator<(const ObjectReference &rhs) const;
  [[nodiscard]] bool operator==(const ObjectReference &rhs) const;

  [[nodiscard]] std::size_t hash() const noexcept;

  void to_stream(std::ostream &) const;
  [[nodiscard]] std::string to_string() const;
};

class Array;
class Dictionary;

template <typename T>
concept ObjectType = std::same_as<T, Boolean> || std::same_as<T, Integer> ||
                     std::same_as<T, Real> || std::same_as<T, StandardString> ||
                     std::same_as<T, HexString> || std::same_as<T, Name> ||
                     std::same_as<T, Array> || std::same_as<T, Dictionary> ||
                     std::same_as<T, ObjectReference>;

class Object final {
public:
  using Holder = std::any;

  static const Object &null() {
    static const Object cache{};
    return cache;
  }

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

  template <ObjectType T> [[nodiscard]] bool is() const {
    return m_holder.type() == typeid(T);
  }
  template <ObjectType T> [[nodiscard]] const T &as() const & {
    return std::any_cast<const T &>(m_holder);
  }
  template <ObjectType T> [[nodiscard]] T &as() & {
    return std::any_cast<T &>(m_holder);
  }
  template <ObjectType T> [[nodiscard]] T &&as() && {
    return std::any_cast<T &&>(std::move(m_holder));
  }
  template <ObjectType T> [[nodiscard]] const T *as_ptr() const & {
    return std::any_cast<T>(&m_holder);
  }
  template <ObjectType T> [[nodiscard]] T *as_ptr() & {
    return std::any_cast<T>(&m_holder);
  }
  template <ObjectType T> [[nodiscard]] std::optional<T> as_opt() const & {
    if (const T *ptr = std::any_cast<T>(&m_holder)) {
      return *ptr;
    }
    return std::nullopt;
  }
  template <ObjectType T> [[nodiscard]] std::optional<T> as_opt() && {
    if (T *ptr = std::any_cast<T>(&m_holder)) {
      return std::move(*ptr);
    }
    return std::nullopt;
  }

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
  [[nodiscard]] const std::string &as_standard_string() const & {
    return as<StandardString>().string;
  }
  [[nodiscard]] const std::string &as_hex_string() const & {
    return as<HexString>().string;
  }
  [[nodiscard]] const std::string &as_name() const & {
    return as<Name>().string;
  }
  [[nodiscard]] const std::string &as_string() const &;
  [[nodiscard]] const Array &as_array() const & { return as<Array>(); }
  [[nodiscard]] const Dictionary &as_dictionary() const & {
    return as<Dictionary>();
  }
  [[nodiscard]] const ObjectReference &as_reference() const {
    return as<ObjectReference>();
  }

  [[nodiscard]] std::string &as_standard_string() & {
    return as<StandardString>().string;
  }
  [[nodiscard]] std::string &as_hex_string() & {
    return as<HexString>().string;
  }
  [[nodiscard]] std::string &as_name() & { return as<Name>().string; }
  [[nodiscard]] std::string &as_string() &;
  Array &as_array() & { return as<Array>(); }
  Dictionary &as_dictionary() & { return as<Dictionary>(); }

  [[nodiscard]] std::string &&as_standard_string() && {
    return std::move(*this).as<StandardString>().string;
  }
  [[nodiscard]] std::string &&as_hex_string() && {
    return std::move(*this).as<HexString>().string;
  }
  [[nodiscard]] std::string &&as_name() && {
    return std::move(*this).as<Name>().string;
  }
  [[nodiscard]] std::string &&as_string() &&;
  Array &&as_array() && { return std::move(*this).as<Array>(); }
  Dictionary &&as_dictionary() && { return std::move(*this).as<Dictionary>(); }

  [[nodiscard]] std::optional<Boolean> as_bool_opt() const {
    return as_opt<Boolean>();
  }
  [[nodiscard]] std::optional<Integer> as_integer_opt() const {
    return as_opt<Integer>();
  }
  [[nodiscard]] std::optional<Real> as_real_opt() const {
    if (is<Real>()) {
      return as<Real>();
    }
    if (const Integer *integer = as_ptr<Integer>()) {
      return static_cast<Real>(*integer);
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<ObjectReference> as_reference_opt() const {
    return as_opt<ObjectReference>();
  }

  [[nodiscard]] std::optional<std::string> as_standard_string_opt() && {
    return std::move(*this).string_opt<StandardString>();
  }
  [[nodiscard]] std::optional<std::string> as_hex_string_opt() && {
    return std::move(*this).string_opt<HexString>();
  }
  [[nodiscard]] std::optional<std::string> as_name_opt() && {
    return std::move(*this).string_opt<Name>();
  }
  [[nodiscard]] std::optional<std::string> as_string_opt() &&;

  [[nodiscard]] const Array *as_array_ptr() const & { return as_ptr<Array>(); }
  [[nodiscard]] const Dictionary *as_dictionary_ptr() const & {
    return as_ptr<Dictionary>();
  }

  Array *as_array_ptr() & { return as_ptr<Array>(); }
  Dictionary *as_dictionary_ptr() & { return as_ptr<Dictionary>(); }

  /// The elements of an array `Object` as doubles, in order (each via
  /// `Object::as_real`). Returns an empty vector when `object` is not an array.
  /// Convenience for the many PDF arrays that are plain number lists
  /// (`/Decode`,
  /// `/Coords`, `/Domain`, `/Background`, a colour-key `/Mask`, ...).
  std::vector<double> as_reals() const;

  void to_stream(std::ostream &) const;
  [[nodiscard]] std::string to_string() const;

private:
  template <ObjectType T>
  [[nodiscard]] std::optional<std::string> string_opt() && {
    if (T *string = as_ptr<T>()) {
      return std::move(string->string);
    }
    return std::nullopt;
  }

  Holder m_holder;
};

class Array final {
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
  [[nodiscard]] bool empty() const { return m_holder.empty(); }
  [[nodiscard]] Holder::iterator begin() { return m_holder.begin(); }
  [[nodiscard]] Holder::iterator end() { return m_holder.end(); }
  [[nodiscard]] Holder::const_iterator begin() const {
    return m_holder.cbegin();
  }
  [[nodiscard]] Holder::const_iterator end() const { return m_holder.cend(); }

  Object &operator[](const std::size_t i) { return m_holder.at(i); }
  const Object &operator[](const std::size_t i) const { return m_holder.at(i); }

  Object &front() { return m_holder.front(); }
  [[nodiscard]] const Object &front() const { return m_holder.front(); }
  Object &back() { return m_holder.back(); }
  [[nodiscard]] const Object &back() const { return m_holder.back(); }

  void to_stream(std::ostream &) const;
  [[nodiscard]] std::string to_string() const;

private:
  Holder m_holder;
};

class Dictionary final {
public:
  using Holder = std::map<std::string, Object>;

  Dictionary() = default;
  explicit Dictionary(Holder holder) : m_holder{std::move(holder)} {}

  Holder &holder() { return m_holder; }
  [[nodiscard]] const Holder &holder() const { return m_holder; }

  [[nodiscard]] std::size_t size() const { return m_holder.size(); }

  using iterator = Holder::iterator;
  using const_iterator = Holder::const_iterator;

  [[nodiscard]] iterator begin() { return m_holder.begin(); }
  [[nodiscard]] iterator end() { return m_holder.end(); }
  [[nodiscard]] const_iterator begin() const { return m_holder.cbegin(); }
  [[nodiscard]] const_iterator end() const { return m_holder.cend(); }

  Object &operator[](const std::string &name) { return m_holder[name]; }
  const Object &operator[](const std::string &name) const {
    return m_holder.at(name);
  }

  [[nodiscard]] Object &at(const std::string &name) {
    return m_holder.at(name);
  }
  [[nodiscard]] const Object &at(const std::string &name) const {
    return m_holder.at(name);
  }

  [[nodiscard]] Holder::iterator find(const std::string &name) {
    return m_holder.find(name);
  }
  [[nodiscard]] Holder::const_iterator find(const std::string &name) const {
    return m_holder.find(name);
  }

  [[nodiscard]] bool has_key(const std::string &name) const {
    return m_holder.contains(name);
  }
  [[nodiscard]] bool has_value(const std::string &name) const {
    const auto it = find(name);
    return it != end() && !it->second.is_null();
  }

  [[nodiscard]] Object &get_emplace(const std::string &name,
                                    Object default_value = Object::null()) {
    const auto [it, success] = m_holder.try_emplace(name, default_value);
    return it->second;
  }
  [[nodiscard]] const Object &
  get(const std::string &name,
      const Object &default_value = Object::null()) const {
    const auto it = find(name);
    return it != end() ? it->second : default_value;
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
