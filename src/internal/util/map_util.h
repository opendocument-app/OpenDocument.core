#ifndef ODR_INTERNAL_UTIL_MAP_H
#define ODR_INTERNAL_UTIL_MAP_H

namespace odr::internal::util::map {
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
bool lookup_map_default(const Map &map, const Key &key, Value &value,
                        const Value &defaultValue) {
  if (!lookupMap(map, key, value)) {
    value = defaultValue;
    return false;
  }
  return true;
}
} // namespace odr::internal::util::map

#endif // ODR_INTERNAL_UTIL_MAP_H
