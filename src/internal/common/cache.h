#ifndef ODR_INTERNAL_COMMON_CACHE_H
#define ODR_INTERNAL_COMMON_CACHE_H

#include <memory>
#include <optional>

namespace odr::internal::common {

template <typename Value, typename Getter> class Cache {
public:
  explicit Cache(Getter getter) : m_getter{getter} {}

  const Value &operator*() { return value(); }

  const Value *operator->() { return &value(); }

  const Value &value() {
    if (!m_cache)
      m_cache = m_getter();
    return *m_cache;
  }

  void reset() { m_cache.reset(); }

private:
  Getter m_getter;
  std::optional<Value> m_cache;
};

template <typename Value, typename Getter> class SharedCache {
public:
  explicit SharedCache(Getter getter)
      : m_cache{std::make_shared<Cache<Value, Getter>>(getter)} {}

  const Value &operator*() { return m_cache->operator*(); }

  const Value *operator->() { return m_cache->operator->(); }

  const Value &value() { return m_cache->value(); }

  void reset() { m_cache->reset(); }

private:
  std::shared_ptr<Cache<Value, Getter>> m_cache;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_CACHE_H
