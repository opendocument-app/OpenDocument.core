#ifndef ODR_MAPUTIL_H
#define ODR_MAPUTIL_H

namespace odr {

namespace MapUtil {
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
} // namespace MapUtil

} // namespace odr

#endif // ODR_MAPUTIL_H
