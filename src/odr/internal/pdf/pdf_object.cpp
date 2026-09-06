#include <odr/internal/pdf/pdf_object.hpp>

#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/util/hash_util.hpp>
#include <odr/internal/util/number_util.hpp>

#include <optional>
#include <ostream>
#include <sstream>

#include <fmt/format.h>
#include <stdexcept>

namespace odr::internal::pdf {

namespace {

/// A name's regular characters (ISO 32000-1 7.3.5): anything else is written
/// as `#` and two hex digits, `#` itself included.
bool name_char_is_regular(const unsigned char c) {
  return c >= 0x21 && c <= 0x7e && c != '#' && c != '/' && c != '%' &&
         c != '(' && c != ')' && c != '<' && c != '>' && c != '[' && c != ']' &&
         c != '{' && c != '}';
}

} // namespace

void StandardString::to_stream(std::ostream &out) const {
  // 7.3.4.2: balance is a property of the whole string, so escape every
  // parenthesis rather than track it. A carriage return would read back as
  // `\n`.
  out << "(";
  for (const char c : string) {
    switch (c) {
    case '\\':
      out << "\\\\";
      break;
    case '(':
      out << "\\(";
      break;
    case ')':
      out << "\\)";
      break;
    case '\r':
      out << "\\r";
      break;
    default:
      out << c;
      break;
    }
  }
  out << ")";
}

std::string StandardString::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

void HexString::to_stream(std::ostream &out) const {
  out << "<" << crypto::util::hex_encode(string) << ">";
}

std::string HexString::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

void Name::to_stream(std::ostream &out) const {
  out << "/";
  for (const char c : string) {
    if (name_char_is_regular(static_cast<unsigned char>(c))) {
      out << c;
    } else {
      out << fmt::format("#{:02X}", static_cast<unsigned char>(c));
    }
  }
}

std::string Name::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

bool ObjectReference::operator<(const ObjectReference &rhs) const {
  return id != rhs.id ? id < rhs.id : gen < rhs.gen;
}

bool ObjectReference::operator==(const ObjectReference &rhs) const {
  return id == rhs.id && gen == rhs.gen;
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

const std::string &Object::as_string() const & {
  if (is_standard_string()) {
    return as_standard_string();
  }
  if (is_hex_string()) {
    return as_hex_string();
  }
  return as_name();
}

std::string &Object::as_string() & {
  if (is_standard_string()) {
    return as_standard_string();
  }
  if (is_hex_string()) {
    return as_hex_string();
  }
  return as_name();
}

std::string &&Object::as_string() && {
  if (is_standard_string()) {
    return std::move(*this).as_standard_string();
  }
  if (is_hex_string()) {
    return std::move(*this).as_hex_string();
  }
  return std::move(*this).as_name();
}

std::optional<std::string> Object::as_string_opt() && {
  if (is_standard_string()) {
    return std::move(*this).as_standard_string_opt();
  }
  if (is_hex_string()) {
    return std::move(*this).as_hex_string_opt();
  }
  if (is_name()) {
    return std::move(*this).as_name_opt();
  }
  return std::nullopt;
}

std::vector<double> Object::as_reals() const {
  std::vector<double> result;
  if (is_array()) {
    const Array &array = as_array();
    result.reserve(array.size());
    for (const Object &item : array) {
      result.push_back(item.as_real());
    }
  }
  return result;
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
    // 7.3.3 has no exponent form, and a stream would carry the host locale
    out << util::number::to_string_significant(as_real(), 10);
  } else if (is_standard_string()) {
    as<StandardString>().to_stream(out);
  } else if (is_hex_string()) {
    as<HexString>().to_stream(out);
  } else if (is_name()) {
    as<Name>().to_stream(out);
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

  bool first = true;
  for (const auto &item : *this) {
    if (!first) {
      out << " ";
    }
    first = false;
    item.to_stream(out);
  }

  out << "]";
}

std::string Array::to_string() const {
  std::ostringstream ss;
  to_stream(ss);
  return ss.str();
}

void Dictionary::to_stream(std::ostream &out) const {
  out << "<<";

  for (const auto &[key, value] : *this) {
    Name(key).to_stream(out);
    out << " ";
    value.to_stream(out);
    out << " ";
  }

  out << ">>";
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
