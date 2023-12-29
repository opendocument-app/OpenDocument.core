#ifndef ODR_INTERNAL_COMMON_PATH_HPP
#define ODR_INTERNAL_COMMON_PATH_HPP

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <typeindex>

namespace odr::internal::common {

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
  [[nodiscard]] Path common_root(const Path &other) const;

  class Iterator final {
  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::string;
    using pointer = const std::string *;
    using reference = const std::string &;

    Iterator();
    explicit Iterator(const std::string &path);
    Iterator(const std::string &path, std::size_t begin);
    explicit Iterator(const Path &path);

    reference operator*() const;
    pointer operator->() const;

    bool operator==(const Iterator &other) const;
    bool operator!=(const Iterator &other) const;

    Iterator &operator++();
    Iterator operator++(int);

  private:
    const std::string *m_path{nullptr};
    std::size_t m_begin{0};
    std::string m_part;

    void fill_();
  };

  Iterator begin() const;
  Iterator end() const;

private:
  std::string m_path;
  std::uint32_t m_upwards;
  std::uint32_t m_downwards;
  bool m_absolute;

  friend std::ostream &operator<<(std::ostream &, const Path &);

  void parent_();
  void join_(const std::string &);
};

} // namespace odr::internal::common

namespace std {
template <> struct hash<::odr::internal::common::Path> {
  std::size_t operator()(const ::odr::internal::common::Path &p) const;
};
} // namespace std

#endif // ODR_INTERNAL_COMMON_PATH_HPP
