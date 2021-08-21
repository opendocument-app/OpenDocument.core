#ifndef ODR_INTERNAL_COMMON_ELEMENT_H
#define ODR_INTERNAL_COMMON_ELEMENT_H

#include <internal/abstract/element.h>
#include <odr/element.h>

namespace odr::internal::common {

template <typename Impl> class Element final : public abstract::Element {
public:
  explicit Element(Impl impl) : m_impl{std::move(impl)} {}

  bool operator==(const abstract::Element & /*rhs*/) const final {
    return false; // TODO
  }

  bool operator!=(const abstract::Element & /*rhs*/) const final {
    return false; // TODO
  }

  [[nodiscard]] ElementType type() const final { return m_impl.type(); }

  [[nodiscard]] odr::Table table() const final {
    return {}; // TODO
  }

  [[nodiscard]] odr::Element parent() const final {
    return odr::Element(Element(m_impl.parent()));
  }

  [[nodiscard]] odr::Element first_child() const final {
    return odr::Element(common::Element(m_impl.first_child()));
  }

  [[nodiscard]] odr::Element previous_sibling() const final {
    return odr::Element(common::Element(m_impl.previous_sibling()));
  }

  [[nodiscard]] odr::Element next_sibling() const final {
    return odr::Element(common::Element(m_impl.next_sibling()));
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const final {
    return m_impl.properties();
  }

  void update_properties(
      std::unordered_map<ElementProperty, std::any> properties) const final {
    m_impl.update_properties(properties);
  }

private:
  Impl m_impl;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_ELEMENT_H
