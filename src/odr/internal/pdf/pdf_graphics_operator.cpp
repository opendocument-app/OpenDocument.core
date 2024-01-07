#include <odr/internal/pdf/pdf_graphics_operator.hpp>

#include <iomanip>
#include <ostream>
#include <sstream>

namespace odr::internal::pdf {

const std::string &SimpleArrayElement::as_string() const {
  if (is_standard_string()) {
    return as_standard_string();
  }
  if (is_hex_string()) {
    return as_hex_string();
  }
  return as_name();
}

void SimpleArrayElement::to_stream(std::ostream &out) const {
  if (is_integer()) {
    out << as_integer();
  } else if (is_real()) {
    out << std::setprecision(4) << as_real();
  } else if (is_string()) {
    // TODO restore original format
    out << as_string();
  } else {
    throw std::runtime_error("unhandled type");
  }
}

std::string SimpleArrayElement::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

void SimpleArray::to_stream(std::ostream &out) const {
  out << "[";

  for (const auto &item : *this) {
    item.to_stream(out);
    out << " ";
  }

  out << " ]";
}

std::string SimpleArray::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

const std::string &GraphicsArgument::as_string() const {
  if (is_standard_string()) {
    return as_standard_string();
  }
  if (is_hex_string()) {
    return as_hex_string();
  }
  return as_name();
}

void GraphicsArgument::to_stream(std::ostream &out) const {
  if (is_integer()) {
    out << as_integer();
  } else if (is_real()) {
    out << std::setprecision(4) << as_real();
  } else if (is_string()) {
    // TODO restore original format
    out << as_string();
  } else if (is_array()) {
    as_array().to_stream(out);
  } else {
    throw std::runtime_error("unhandled type");
  }
}

std::string GraphicsArgument::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

} // namespace odr::internal::pdf

namespace odr::internal {

std::ostream &pdf::operator<<(std::ostream &out,
                              const SimpleArrayElement &element) {
  element.to_stream(out);
  return out;
}

std::ostream &pdf::operator<<(std::ostream &out, const SimpleArray &array) {
  array.to_stream(out);
  return out;
}

std::ostream &pdf::operator<<(std::ostream &out,
                              const GraphicsArgument &argument) {
  argument.to_stream(out);
  return out;
}

} // namespace odr::internal
