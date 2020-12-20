#ifndef ODR_COMMON_XMLPROPERTIES_H
#define ODR_COMMON_XMLPROPERTIES_H

#include <common/Property.h>
#include <exception>
#include <pugixml.hpp>

namespace odr::common {

class XmlAttributeProperty final : public Property {
public:
  explicit XmlAttributeProperty(pugi::xml_attribute attribute);

  bool readonly() const noexcept final;
  bool optional() const noexcept final;

  std::optional<std::string> value() const final;

  void set(std::optional<std::string> value) final;

private:
  mutable pugi::xml_attribute m_attribute;
};

class XmlOptionalAttributeProperty final : public Property {
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

} // namespace odr::common

#endif // ODR_COMMON_XMLPROPERTIES_H
