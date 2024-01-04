#ifndef ODR_INTERNAL_PDF_GRAPHICS_OPERATOR_HPP
#define ODR_INTERNAL_PDF_GRAPHICS_OPERATOR_HPP

#include <odr/internal/pdf/pdf_object.hpp>

#include <string>
#include <variant>
#include <vector>

namespace odr::internal::pdf {

class SimpleArrayElement {
public:
  using Holder = std::variant<Integer, Real, std::string>;

  SimpleArrayElement(Integer integer) : m_holder{integer} {}
  SimpleArrayElement(Real real) : m_holder{real} {}
  SimpleArrayElement(std::string string) : m_holder{std::move(string)} {}

  Holder &holder() { return m_holder; }
  const Holder &holder() const { return m_holder; }

  bool is_integer() const { return is<Integer>(); }
  bool is_real() const { return is<Real>(); }
  bool is_string() const { return is<std::string>(); }

  Integer as_integer() const { return as<Integer>(); }
  Real as_real() const { return as<Real>(); }
  const std::string &as_string() const { return as<std::string>(); }

private:
  Holder m_holder;

  template <typename T> bool is() const {
    return std::holds_alternative<T>(m_holder);
  }
  template <typename T> const T &as() const { return std::get<T>(m_holder); }
};

class SimpleArray {
public:
  using Holder = std::vector<SimpleArrayElement>;

  SimpleArray() = default;
  explicit SimpleArray(Holder holder) : m_holder{std::move(holder)} {}

  Holder &holder() { return m_holder; }
  const Holder &holder() const { return m_holder; }

  std::size_t size() const { return m_holder.size(); }
  Holder::const_iterator begin() const { return std::begin(m_holder); }
  Holder::const_iterator end() const { return std::end(m_holder); }

  const SimpleArrayElement &operator[](std::size_t i) const {
    return m_holder.at(i);
  }

private:
  Holder m_holder;
};

class GraphicsArgument {
public:
  using Holder = std::variant<Integer, Real, std::string, SimpleArray>;

  GraphicsArgument(Integer integer) : m_holder{integer} {}
  GraphicsArgument(Real real) : m_holder{real} {}
  GraphicsArgument(std::string string) : m_holder{std::move(string)} {}
  GraphicsArgument(SimpleArray array) : m_holder(std::move(array)) {}

  Holder &holder() { return m_holder; }
  const Holder &holder() const { return m_holder; }

  bool is_integer() const { return is<Integer>(); }
  bool is_real() const { return is<Real>(); }
  bool is_string() const { return is<std::string>(); }
  bool is_array() const { return is<SimpleArray>(); }

  Integer as_integer() const { return as<Integer>(); }
  Real as_real() const { return as<Real>(); }
  const std::string &as_string() const { return as<std::string>(); }
  const SimpleArray &as_array() const { return as<SimpleArray>(); }

private:
  Holder m_holder;

  template <typename T> bool is() const {
    return std::holds_alternative<T>(m_holder);
  }
  template <typename T> const T &as() const { return std::get<T>(m_holder); }
};

struct GraphicsOperator {
  using Argument = GraphicsArgument;
  using Arguments = std::vector<Argument>;

  std::string name;
  Arguments arguments;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_GRAPHICS_OPERATOR_HPP
