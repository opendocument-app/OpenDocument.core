#include <odr/internal/pdf/pdf_object.hpp>

#include <odr/internal/util/hash_util.hpp>

#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>

namespace odr::internal::pdf {

void StandardString::to_stream(std::ostream &out) const {
  // TODO escape
  out << "(" << string << ")";
}

std::string StandardString::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

void HexString::to_stream(std::ostream &out) const {
  // TODO hex
  out << "<" << string << ">";
}

std::string HexString::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

void Name::to_stream(std::ostream &out) const { out << "/" << string; }

std::string Name::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

bool ObjectReference::operator<(const ObjectReference &rhs) const {
  return id != rhs.id ? id < rhs.id : gen < rhs.gen;
}

std::size_t ObjectReference::hash() const noexcept {
  std::size_t result = 0;
  util::hash::hash_combine(result, gen, id);
  return result;
}

void ObjectReference::to_stream(std::ostream &out) const {
  out << id << " " << gen << " R";
}

std::string ObjectReference::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

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
    as<const StandardString &>().to_stream(out);
  } else if (is_hex_string()) {
    as<const HexString &>().to_stream(out);
  } else if (is_name()) {
    as<const Name &>().to_stream(out);
  } else if (is_array()) {
    as_array().to_stream(out);
  } else if (is_dictionary()) {
    as_dictionary().to_stream(out);
  } else if (is_reference()) {
    as_reference().to_stream(out);
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

std::ostream &pdf::operator<<(std::ostream &out,
                              const StandardString &standard_string) {
  standard_string.to_stream(out);
  return out;
}

std::ostream &pdf::operator<<(std::ostream &out, const HexString &hex_string) {
  hex_string.to_stream(out);
  return out;
}

std::ostream &pdf::operator<<(std::ostream &out, const Name &name) {
  name.to_stream(out);
  return out;
}

std::ostream &pdf::operator<<(std::ostream &out,
                              const ObjectReference &object_reference) {
  object_reference.to_stream(out);
  return out;
}

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

std::size_t std::hash<odr::internal::pdf::ObjectReference>::operator()(
    const odr::internal::pdf::ObjectReference &k) const noexcept {
  return k.hash();
}
