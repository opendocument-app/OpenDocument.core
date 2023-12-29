#ifndef ODR_ARCHIVE_HPP
#define ODR_ARCHIVE_HPP

#include <memory>
#include <string>

namespace odr::internal::abstract {
class Archive;
} // namespace odr::internal::abstract

namespace odr {
class Filesystem;

class Archive {
public:
  explicit Archive(std::shared_ptr<internal::abstract::Archive>);

  [[nodiscard]] explicit operator bool() const;

  [[nodiscard]] Filesystem filesystem() const;

  void save(const std::string &path) const;

private:
  std::shared_ptr<internal::abstract::Archive> m_impl;
};

} // namespace odr

#endif // ODR_ARCHIVE_HPP
