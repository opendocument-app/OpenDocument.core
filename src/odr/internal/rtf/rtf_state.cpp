#include <odr/internal/rtf/rtf_state.hpp>

#include <stdexcept>

namespace odr::internal::rtf {

State::State() : m_stack(1) {}

State::Group &State::current() { return m_stack.back(); }

const State::Group &State::current() const { return m_stack.back(); }

void State::save() {
  if (m_stack.size() >= max_depth) {
    throw std::runtime_error("rtf: group nesting too deep");
  }
  m_stack.push_back(m_stack.back());
}

void State::restore() {
  if (m_stack.size() > 1) {
    m_stack.pop_back();
  }
}

std::size_t State::depth() const noexcept { return m_stack.size(); }

} // namespace odr::internal::rtf
