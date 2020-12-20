#ifndef ODR_COMMON_PROPERTY_H
#define ODR_COMMON_PROPERTY_H

#include <optional>
#include <string>

namespace odr::common {

class Property {
public:
  virtual ~Property() = default;

  virtual bool readonly() const noexcept = 0;
  virtual bool optional() const noexcept = 0;

  virtual std::optional<std::string> value() const = 0;

  virtual void set(std::optional<std::string> value) = 0;
};

class ConstProperty final : public Property {
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

} // namespace odr::common

#endif // ODR_COMMON_PROPERTY_H
