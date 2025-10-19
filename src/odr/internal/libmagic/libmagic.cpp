#include <odr/internal/libmagic/libmagic.hpp>

#include <odr/global_params.hpp>

#include <memory>

#include <magic.h>

namespace odr::internal {

namespace {

void magic_deleter(const magic_t magic_cookie) {
  if (magic_cookie != nullptr) {
    magic_close(magic_cookie);
  }
}

magic_t get_magic_cookie() {
  using Holder =
      std::unique_ptr<std::remove_pointer_t<magic_t>, decltype(&magic_deleter)>;
  static Holder magic_cookie(nullptr, &magic_deleter);

  if (magic_cookie) {
    return magic_cookie.get();
  }

  magic_cookie = Holder(magic_open(MAGIC_MIME_TYPE), &magic_deleter);
  if (!magic_cookie) {
    throw std::runtime_error("magic_open failed");
  }
  if (magic_load(magic_cookie.get(), GlobalParams::libmagic_path().c_str()) ==
      0) {
    return magic_cookie.get();
  }
  if (magic_load(magic_cookie.get(), nullptr) == 0) {
    return magic_cookie.get();
  }
  magic_cookie.reset();
  throw std::runtime_error("magic_load failed");
}

} // namespace

const char *libmagic::mimetype(const std::string &path) {
  const magic_t magic_cookie = get_magic_cookie();
  return magic_file(magic_cookie, path.c_str());
}

} // namespace odr::internal
