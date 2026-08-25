#pragma once

#include <cstddef>
#include <stdexcept>

namespace odr::internal::iwork {

/// What one parse may expand to. A reference list may name the same object any
/// number of times, so what a walk builds is spent against a limit rather than
/// left to grow with what a few bytes of references ask for.
class Budget final {
public:
  void spend_element() {
    if (++m_elements > element_limit) {
      throw std::runtime_error("iwork: document holds too many elements");
    }
  }

  void spend_text(const std::size_t bytes) {
    m_text += bytes;
    if (m_text > text_limit) {
      throw std::runtime_error("iwork: document holds too much text");
    }
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
