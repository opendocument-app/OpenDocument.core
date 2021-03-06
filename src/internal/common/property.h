#ifndef ODR_INTERNAL_COMMON_PROPERTY_H
#define ODR_INTERNAL_COMMON_PROPERTY_H

#include <internal/abstract/property.h>
#include <pugixml.hpp>

namespace odr::internal::common {

// TODO remove
class ConstProperty final : public abstract::Property {
public:
  ConstProperty();
  explicit ConstProperty(std::optional<std::string>);

  bool readonly() const noexcept final;
  bool optional() const noexcept final;

  std::optional<std::string> value() const final;

  void set(std::optional<std::string> value) final;

private:
  std::optional<std::string> m_value;
};

template <typename Get = void, typename Set = void>
class LambdaProperty final : public abstract::Property {
public:
  // TODO if no `Get`
  LambdaProperty() = default;
  // TODO if no `Set`
  explicit LambdaProperty(Get get) : m_get{std::move(get)} {}
  // TODO if `Get` and `Set`
  LambdaProperty(Get get, Set set)
      : m_get{std::move(get)}, m_set{std::move(set)} {}

  bool readonly() const noexcept final {
    return true; // TODO if `m_get` only
  }

  bool optional() const noexcept final {
    return true; // TODO if `m_get` not optional
  }

  std::optional<std::string> value() const final { return m_get(); }

  void set(std::optional<std::string> value) final { m_set(std::move(value)); }

private:
  Get m_get;
  Set m_set;
};

class XmlAttributeProperty final : public abstract::Property {
public:
  explicit XmlAttributeProperty(pugi::xml_attribute attribute);

  bool readonly() const noexcept final;
  bool optional() const noexcept final;

  std::optional<std::string> value() const final;

  void set(std::optional<std::string> value) final;

private:
  mutable pugi::xml_attribute m_attribute;
};

class XmlOptionalAttributeProperty final : public abstract::Property {
public:
  XmlOptionalAttributeProperty(pugi::xml_node node, std::string attributeName);

  bool readonly() const noexcept final;
  bool optional() const noexcept final;

  std::optional<std::string> value() const final;

  void set(std::optional<std::string> value) final;

private:
  pugi::xml_node m_node;
  std::string m_attributeName;

  pugi::xml_attribute attribute() const;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_PROPERTY_H
