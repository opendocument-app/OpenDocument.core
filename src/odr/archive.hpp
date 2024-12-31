#pragma once

#include <memory>
#include <string>

namespace odr::internal::abstract {
class Archive;
} // namespace odr::internal::abstract

namespace odr {
class Filesystem;

/// @brief Represents an archive file.
class Archive {
public:
  explicit Archive(std::shared_ptr<internal::abstract::Archive>);

  [[nodiscard]] explicit operator bool() const;

  [[nodiscard]] Filesystem filesystem() const;

  void save(std::ostream &out) const;

private:
  std::shared_ptr<internal::abstract::Archive> m_impl;
};

} // namespace odr
