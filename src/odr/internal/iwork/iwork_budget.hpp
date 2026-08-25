#pragma once

#include <cstddef>

namespace odr::internal::iwork {

/// What one parse may expand to. An `.iwa` is an object graph, so a reference
/// list may name the same object any number of times and `Package::object`
/// hands every repeat back from its cache — a few kilobytes of references
/// would otherwise build elements and copy text without bound. Spending
/// against a budget keeps such a package the thrown error every caller already
/// handles rather than an allocation the process dies on.
class Budget final {
public:
  void spend_element();
  void spend_text(std::size_t bytes);

private:
  /// Far above what an authored document reaches, and far below what the
  /// process cannot hold.
  static constexpr std::size_t element_limit = 1'000'000;
  static constexpr std::size_t text_limit = std::size_t{64} * 1024 * 1024;

  std::size_t m_elements{};
  std::size_t m_text{};
};

} // namespace odr::internal::iwork
