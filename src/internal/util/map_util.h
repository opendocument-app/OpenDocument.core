#ifndef ODR_INTERNAL_UTIL_MAP_H
#define ODR_INTERNAL_UTIL_MAP_H

namespace odr::internal::util::map {
template <typename Map, typename Key, typename Value>
bool lookupMap(const Map &map, const Key &key, Value &value) {
  const auto it = map.find(key);
  if (it == std::end(map)) {
    return false;
  }
  value = it->second;
  return true;
}

template <typename Map, typename Key, typename Value>
bool lookup_map_default(const Map &map, const Key &key, Value &value,
                        const Value &default_value) {
  if (!lookupMap(map, key, value)) {
    value = default_value;
    return false;
  }
  return true;
}
} // namespace odr::internal::util::map

#endif // ODR_INTERNAL_UTIL_MAP_H
