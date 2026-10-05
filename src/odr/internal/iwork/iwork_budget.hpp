#pragma once

#include <cstddef>
#include <stdexcept>

namespace odr::internal::iwork {

/// Bounds the elements and text expanded by one document parse.
class Budget final {
public:
  void spend_element() {
    if (m_elements == element_limit) {
      throw std::runtime_error("iwork: document holds too many elements");
    }
    ++m_elements;
  }

  void spend_text(const std::size_t bytes) {
    if (bytes > text_limit - m_text) {
      throw std::runtime_error("iwork: document holds too much text");
    }
    m_text += bytes;
  }

private:
  /// Far above what an authored document reaches, and far below what the
  /// process cannot hold.
  static constexpr std::size_t element_limit = 1'000'000;
  static constexpr std::size_t text_limit = std::size_t{64} * 1024 * 1024;

  std::size_t m_elements{};
  std::size_t m_text{};
};

} // namespace odr::internal::iwork
