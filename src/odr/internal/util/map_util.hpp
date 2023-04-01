#ifndef ODR_INTERNAL_UTIL_MAP_H
#define ODR_INTERNAL_UTIL_MAP_H

namespace odr::internal::util::map {

template <typename Map, typename Key>
bool lookup(const Map &map, const Key &key, typename Map::mapped_type &value) {
  const auto it = map.find(key);
  if (it == std::end(map)) {
    return false;
  }
  value = it->second;
  return true;
}

template <typename Map, typename Key>
bool lookup_default(const Map &map, const Key &key,
                    typename Map::mapped_type &value,
                    const typename Map::mapped_type &default_value) {
  if (!lookup(map, key, value)) {
    value = default_value;
    return false;
  }
  return true;
}

template <typename Map, typename Key>
typename Map::mapped_type
lookup_default(const Map &map, const Key &key,
               const typename Map::mapped_type &default_value) {
  const auto it = map.find(key);
  if (it == std::end(map)) {
    return default_value;
  }
  return it->second;
}

template <typename Map, typename Key>
typename Map::const_iterator lookup_greater_or_equals(const Map &map,
                                                      const Key &key) {
  auto lower_bound = map.lower_bound(key);
  if (lower_bound == std::begin(map)) {
    return lower_bound;
  }
  return lower_bound;
}

} // namespace odr::internal::util::map

#endif // ODR_INTERNAL_UTIL_MAP_H
