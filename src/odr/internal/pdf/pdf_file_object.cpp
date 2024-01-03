#include <odr/internal/pdf/pdf_file_object.hpp>

namespace odr::internal::pdf {

Object::Object(Array array) : m_holder{std::move(array)} {}

Object::Object(Dictionary dictionary) : m_holder{std::move(dictionary)} {}

} // namespace odr::internal::pdf
