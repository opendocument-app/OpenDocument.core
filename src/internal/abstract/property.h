#ifndef ODR_INTERNAL_ABSTRACT_PROPERTY_H
#define ODR_INTERNAL_ABSTRACT_PROPERTY_H

#include <optional>
#include <string>

namespace odr::internal::abstract {

class Property {
public:
  virtual ~Property() = default;

  [[nodiscard]] virtual bool readonly() const noexcept = 0;
  [[nodiscard]] virtual bool optional() const noexcept = 0;

  [[nodiscard]] virtual std::optional<std::string> value() const = 0;

  virtual void set(std::optional<std::string> value) = 0;
};

// TODO remove
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

template <typename Get = void, typename Set = void>
class LambdaProperty final : public Property {
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

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_PROPERTY_H
