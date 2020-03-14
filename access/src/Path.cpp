#include <access/Path.h>
#include <algorithm>
#include <stdexcept>

namespace odr {
namespace access {

Path::Path() noexcept : Path("") {}

Path::Path(const char *path) : Path(std::string(path)) {}

Path::Path(const std::string &path) {
  // TODO throw on illegal chars
  // TODO remove forward slash
  if (path.rfind("/..", 0) == 0)
    throw std::invalid_argument("path");

  absolute_ = !path.empty() && (path[0] == '/');
  path_ = absolute_ ? "/" : "";
  upwards_ = 0;
  downwards_ = 0;

  std::size_t pos = absolute_ ? 1 : 0;
  while (pos < path.size()) {
    std::size_t next = path.find('/', pos);
    if (next == std::string::npos)
      next = path.size();
    join_(path.substr(pos, next - pos));
    pos = next + 1;
  }
}

void Path::parent_() {
  if (downwards_ > 0) {
    --downwards_;
    if (downwards_ == 0)
      path_ = "";
    else {
      const auto pos = path_.rfind('/');
      path_ = path_.substr(0, pos);
    }
  } else if (!absolute_) {
    ++upwards_;
    path_ = "../" + path_;
  } else {
    throw std::invalid_argument("absolute path violation");
  }
}

void Path::join_(const std::string &child) {
  if (child == ".")
    return;
  if (child == "..")
    parent_();
  else {
    if (downwards_ == 0)
      path_ += child;
    else
      path_ += "/" + child;
    ++downwards_;
  }
}

bool Path::operator==(const Path &b) const noexcept {
  if (absolute_ != b.absolute_)
    return false;
  if (!absolute_ && (upwards_ != b.upwards_))
    return false;
  return (downwards_ == b.downwards_) && (path_ == b.path_);
}

bool Path::operator!=(const Path &b) const noexcept {
  if (absolute_ != b.absolute_)
    return true;
  if (!absolute_ && (upwards_ != b.upwards_))
    return true;
  return (downwards_ != b.downwards_) || (path_ != b.path_);
}

bool Path::operator<(const Path &b) const noexcept {
  // TODO could be improved
  return path_ < b.path_;
}

bool Path::operator>(const Path &b) const noexcept {
  // TODO could be improved
  return path_ > b.path_;
}

Path::operator std::string() const noexcept { return path_; }

Path::operator const std::string &() const noexcept { return path_; }

const std::string &Path::string() const noexcept { return path_; }

std::size_t Path::hash() const noexcept {
  return std::hash<std::string>{}(path_);
}

bool Path::isVisible() const noexcept { return absolute_ || (upwards_ == 0); }

bool Path::isEscaping() const noexcept { return !absolute_ && (upwards_ > 0); }

bool Path::childOf(const Path &b) const { return b.parentOf(*this); }

bool Path::parentOf(const Path &b) const {
  if (absolute_ != b.absolute_)
    throw std::invalid_argument("cannot compare absolute and relative path");
  // TODO we need to check upwards as well
  return (downwards_ + 1 == b.downwards_) && (b.path_.rfind(path_, 0) == 0);
}

bool Path::ancestorOf(const Path &b) const { return b.descendantOf(*this); }

bool Path::descendantOf(const Path &b) const {
  if (absolute_ != b.absolute_)
    throw std::invalid_argument("cannot compare absolute and relative path");
  // TODO we need to check upwards as well
  return (downwards_ < b.downwards_) && (b.path_.rfind(path_, 0) == 0);
}

std::string Path::basename() const noexcept {
  const auto find = path_.rfind('/');
  if (find == std::string::npos)
    return path_;
  return path_.substr(find + 1);
}

std::string Path::extension() const noexcept {
  auto bn = basename();
  const auto pos = bn.rfind('.');
  if (pos == std::string::npos)
    return "";
  return bn.substr(pos + 1);
}

std::string Path::fullExtension() const noexcept {
  auto bn = basename();
  const auto pos = bn.find('.');
  if (pos == std::string::npos)
    return "";
  return bn.substr(pos + 1);
}

Path Path::parent() const {
  Path result(*this);
  result.parent_();
  return result;
}

Path Path::join(const Path &b) const {
  if (b.absolute_)
    throw std::invalid_argument("cannot join an absolute path");
  // TODO could be done directly
  const std::string result = path_ + "/" + b.path_;
  return Path(result);
}

std::ostream &operator<<(std::ostream &os, const Path &p) {
  return os << p.path_;
}

} // namespace access
} // namespace odr
