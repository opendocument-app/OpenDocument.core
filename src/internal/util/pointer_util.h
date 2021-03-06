#ifndef ODR_INTERNAL_UTIL_POINTER_H
#define ODR_INTERNAL_UTIL_POINTER_H

namespace odr::internal::util {

template <typename To, typename From>
std::unique_ptr<To> dynamic_pointer_cast(std::unique_ptr<From> &&p) {
  To *cast = dynamic_cast<To *>(p.get());
  if (cast == nullptr) {
    return std::unique_ptr<To>();
  }
  std::unique_ptr<To> result(cast);
  p.release();
  return result;
}

template <typename To, typename From, typename Deleter>
std::unique_ptr<To, Deleter>
dynamic_pointer_cast(std::unique_ptr<From, Deleter> &&p) {
  To *cast = dynamic_cast<To *>(p.get());
  if (cast == nullptr) {
    return std::unique_ptr<To, Deleter>();
  }
  std::unique_ptr<To, Deleter> result(cast, std::move(p.get_deleter()));
  p.release();
  return result;
}

} // namespace odr::internal::util

#endif // ODR_INTERNAL_UTIL_POINTER_H
