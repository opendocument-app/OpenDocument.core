#ifndef ODR_COMMON_PATH_H
#define ODR_COMMON_PATH_H

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <typeindex>

namespace odr::common {

class Path final {
public:
  Path() noexcept;
  Path(const char *c_string);
  Path(const std::string &string);
  Path(const std::filesystem::path &path);

  bool operator==(const Path &other) const noexcept;
  bool operator!=(const Path &other) const noexcept;
  bool operator<(const Path &other) const noexcept;
  bool operator>(const Path &other) const noexcept;

  operator std::string() const noexcept;
  operator std::filesystem::path() const noexcept;
  operator const std::string &() const noexcept;
  [[nodiscard]] const std::string &string() const noexcept;
  [[nodiscard]] std::filesystem::path path() const noexcept;
  [[nodiscard]] std::size_t hash() const noexcept;

  [[nodiscard]] bool root() const noexcept;
  [[nodiscard]] bool absolute() const noexcept;
  [[nodiscard]] bool relative() const noexcept;
  [[nodiscard]] bool visible() const noexcept;
  [[nodiscard]] bool escaping() const noexcept;
  [[nodiscard]] bool child_of(const Path &other) const;
  [[nodiscard]] bool parent_of(const Path &other) const;
  [[nodiscard]] bool ancestor_of(const Path &other) const;
  [[nodiscard]] bool descendant_of(const Path &other) const;

  [[nodiscard]] std::string basename() const noexcept;
  [[nodiscard]] std::string extension() const noexcept;

  [[nodiscard]] Path parent() const;
  [[nodiscard]] Path join(const Path &other) const;
  [[nodiscard]] Path rebase(const Path &on) const;

private:
  std::string m_path;
  std::uint32_t m_upwards;
  std::uint32_t m_downwards;
  bool m_absolute;

  friend struct ::std::hash<Path>;
  friend std::ostream &operator<<(std::ostream &, const Path &);

  void parent_();
  void join_(const std::string &);
};

} // namespace odr::common

namespace std {
template <> struct hash<::odr::common::Path> {
  std::size_t operator()(const ::odr::common::Path &p) const;
};
} // namespace std

#endif // ODR_COMMON_PATH_H
