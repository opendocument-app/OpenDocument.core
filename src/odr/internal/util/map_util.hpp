#ifndef ODR_INTERNAL_UTIL_MAP_H
#define ODR_INTERNAL_UTIL_MAP_H

namespace odr::internal::util::map {
template <typename Map, typename Key>
bool lookup_map(const Map &map, const Key &key,
                typename Map::mapped_type &value) {
  const auto it = map.find(key);
  if (it == std::end(map)) {
    return false;
  }
  value = it->second;
  return true;
}

template <typename Map, typename Key>
bool lookup_map_default(const Map &map, const Key &key,
                        typename Map::mapped_type &value,
                        const typename Map::mapped_type &default_value) {
  if (!lookup_map(map, key, value)) {
    value = default_value;
    return false;
  }
  return true;
}

template <typename Map, typename Key>
typename Map::mapped_type
lookup_map_default(const Map &map, const Key &key,
                   const typename Map::mapped_type &default_value) {
  const auto it = map.find(key);
  if (it == std::end(map)) {
    return default_value;
  }
  return it->second;
}
} // namespace odr::internal::util::map

#endif // ODR_INTERNAL_UTIL_MAP_H
