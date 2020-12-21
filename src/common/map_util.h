#ifndef ODR_COMMON_MAP_UTIL_H
#define ODR_COMMON_MAP_UTIL_H

namespace odr::common::MapUtil {
template <typename Map, typename Key, typename Value>
bool lookupMap(const Map &map, const Key &key, Value &value) {
  const auto it = map.find(key);
  if (it == map.end()) {
    return false;
  }
  value = it->second;
  return true;
}

template <typename Map, typename Key, typename Value>
bool lookupMapDefault(const Map &map, const Key &key, Value &value,
                      const Value &defaultValue) {
  if (!lookupMap(map, key, value)) {
    value = defaultValue;
    return false;
  }
  return true;
}
} // namespace odr::common::MapUtil

#endif // ODR_COMMON_MAP_UTIL_H
