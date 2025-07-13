#pragma once

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <typeindex>

namespace odr::internal {

class AbsPath;
class RelPath;

class Path {
public:
  Path() noexcept;
  explicit Path(const char *c_string);
  explicit Path(const std::string &string);
  explicit Path(std::string_view string_view);
  explicit Path(const std::filesystem::path &path);

  bool operator==(const Path &other) const noexcept;
  bool operator!=(const Path &other) const noexcept;
  bool operator<(const Path &other) const noexcept;
  bool operator>(const Path &other) const noexcept;

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

  [[nodiscard]] AbsPath as_absolute() const &;
  [[nodiscard]] AbsPath as_absolute() &&;
  [[nodiscard]] RelPath as_relative() const &;
  [[nodiscard]] RelPath as_relative() &&;

  [[nodiscard]] AbsPath make_absolute() const;

  [[nodiscard]] std::string basename() const noexcept;
  [[nodiscard]] std::string extension() const noexcept;

  [[nodiscard]] Path parent() const;
  [[nodiscard]] Path join(const RelPath &other) const;
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

  [[nodiscard]] Iterator begin() const;
  [[nodiscard]] Iterator end() const;

private:
  std::string m_path;
  std::uint32_t m_upwards;
  std::uint32_t m_downwards;
  bool m_absolute;

  friend std::ostream &operator<<(std::ostream &os, const Path &p) {
    return os << p.m_path;
  }

  void parent_();
  void join_(const std::string &);
};

class AbsPath final : public Path {
public:
  AbsPath() noexcept;
  explicit AbsPath(const char *c_string);
  explicit AbsPath(const std::string &string);
  explicit AbsPath(std::string_view string_view);
  explicit AbsPath(const std::filesystem::path &path);
  explicit AbsPath(Path path);

  [[nodiscard]] AbsPath parent() const;
  [[nodiscard]] AbsPath join(const RelPath &other) const;
  [[nodiscard]] RelPath rebase(const AbsPath &on) const;
  [[nodiscard]] AbsPath common_root(const AbsPath &other) const;
};

class RelPath final : public Path {
public:
  RelPath() noexcept;
  explicit RelPath(const char *c_string);
  explicit RelPath(const std::string &string);
  explicit RelPath(std::string_view string_view);
  explicit RelPath(const std::filesystem::path &path);
  explicit RelPath(Path path);

  [[nodiscard]] RelPath parent() const;
  [[nodiscard]] RelPath join(const RelPath &other) const;
  [[nodiscard]] RelPath rebase(const RelPath &on) const;
  [[nodiscard]] RelPath common_root(const RelPath &other) const;
};

} // namespace odr::internal

namespace std {
template <> struct hash<::odr::internal::Path> {
  std::size_t operator()(const ::odr::internal::Path &p) const;
};
template <> struct hash<::odr::internal::AbsPath> {
  std::size_t operator()(const ::odr::internal::AbsPath &p) const;
};
template <> struct hash<::odr::internal::RelPath> {
  std::size_t operator()(const ::odr::internal::RelPath &p) const;
};
} // namespace std
