#include <odr/internal/iwork/iwork_budget.hpp>

#include <stdexcept>

namespace odr::internal::iwork {

void Budget::spend_element() {
  if (++m_elements > element_limit) {
    throw std::runtime_error("iwork: document holds too many elements");
  }
}

void Budget::spend_text(const std::size_t bytes) {
  m_text += bytes;
  if (m_text > text_limit) {
    throw std::runtime_error("iwork: document holds too much text");
  }
}

} // namespace odr::internal::iwork
