#include <odr/internal/pdf/pdf_object.hpp>

#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>

namespace odr::internal::pdf {

Object::Object(Array array) : m_holder{std::move(array)} {}

Object::Object(Dictionary dictionary) : m_holder{std::move(dictionary)} {}

const std::string &Object::as_string() const {
  if (is_standard_string()) {
    return as_standard_string();
  }
  if (is_hex_string()) {
    return as_hex_string();
  }
  return as_name();
}

void Object::to_stream(std::ostream &out) const {
  if (is_null()) {
    out << "null";
  } else if (is_bool()) {
    if (as_bool()) {
      out << "true";
    } else {
      out << "false";
    }
  } else if (is_integer()) {
    out << as_integer();
  } else if (is_real()) {
    out << std::setprecision(4) << as_real();
  } else if (is_standard_string()) {
    // TODO escape
    out << "(" << as_standard_string() << ")";
  } else if (is_hex_string()) {
    // TODO hex
    out << "<" << as_hex_string() << ">";
  } else if (is_name()) {
    out << "/" << as_name();
  } else if (is_array()) {
    as_array().to_stream(out);
  } else if (is_dictionary()) {
    as_dictionary().to_stream(out);
  } else if (is_reference()) {
    out << as_reference().id << " " << as_reference().gen << " R";
  } else {
    throw std::runtime_error("unhandled type");
  }
}

std::string Object::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

void Array::to_stream(std::ostream &out) const {
  out << "[";

  for (const auto &item : *this) {
    item.to_stream(out);
    out << " ";
  }

  out << " ]";
}

std::string Array::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

void Dictionary::to_stream(std::ostream &out) const {
  out << "<<";

  for (const auto &[key, value] : *this) {
    out << "/" << key;
    out << " ";
    value.to_stream(out);
    out << " ";
  }

  out << " >>";
}

std::string Dictionary::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

} // namespace odr::internal::pdf

namespace odr::internal {

std::ostream &pdf::operator<<(std::ostream &out, const Object &object) {
  object.to_stream(out);
  return out;
}

std::ostream &pdf::operator<<(std::ostream &out, const Array &array) {
  array.to_stream(out);
  return out;
}

std::ostream &pdf::operator<<(std::ostream &out, const Dictionary &dictionary) {
  dictionary.to_stream(out);
  return out;
}

} // namespace odr::internal
