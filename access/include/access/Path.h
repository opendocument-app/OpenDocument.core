#ifndef ODR_ACCESS_PATH_H
#define ODR_ACCESS_PATH_H

#include <cstdint>
#include <iostream>
#include <string>
#include <typeindex>

namespace odr {
namespace access {

class Path final {
public:
  Path() noexcept;
  Path(const char *);
  Path(const std::string &);
  Path(const Path &) = default;
  Path(Path &&) = default;
  ~Path() = default;

  Path &operator=(const Path &) = default;
  Path &operator=(Path &&) = default;

  bool operator==(const Path &) const noexcept;
  bool operator!=(const Path &) const noexcept;
  bool operator<(const Path &) const noexcept;
  bool operator>(const Path &) const noexcept;

  operator std::string() const noexcept;
  operator const std::string &() const noexcept;
  const std::string &string() const noexcept;
  std::size_t hash() const noexcept;

  bool isAbsolute() const noexcept { return absolute_; }
  bool isRelative() const noexcept { return !absolute_; }
  bool isVisible() const noexcept;
  bool isEscaping() const noexcept;
  bool childOf(const Path &) const;
  bool parentOf(const Path &) const;
  bool ancestorOf(const Path &) const;
  bool descendantOf(const Path &) const;

  std::string basename() const noexcept;
  std::string extension() const noexcept;
  std::string fullExtension() const noexcept;

  Path parent() const;
  Path join(const Path &) const;

private:
  std::string path_;
  std::uint32_t upwards_;
  std::uint32_t downwards_;
  bool absolute_;

  friend struct ::std::hash<Path>;
  friend std::ostream &operator<<(std::ostream &, const Path &);

  void parent_();
  void join_(const std::string &);
};

} // namespace access
} // namespace odr

namespace std {
template <> struct hash<::odr::access::Path> {
  std::size_t operator()(const ::odr::access::Path &p) const {
    return p.hash();
  }
};
} // namespace std

#endif // ODR_ACCESS_PATH_H
