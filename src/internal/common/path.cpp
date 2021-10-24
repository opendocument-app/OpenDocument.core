#include <internal/common/path.h>
#include <stdexcept>

std::size_t std::hash<::odr::internal::common::Path>::operator()(
    const ::odr::internal::common::Path &p) const {
  return p.hash();
}

namespace odr::internal::common {

Path::Path() noexcept : Path("") {}

Path::Path(const char *path) : Path(std::string(path)) {}

Path::Path(const std::string &path) {
  // TODO throw on illegal chars
  // TODO remove forward slash
  if (path.rfind("/..", 0) == 0) {
    throw std::invalid_argument("path");
  }

  m_absolute = !path.empty() && (path[0] == '/');
  m_path = m_absolute ? "/" : "";
  m_upwards = 0;
  m_downwards = 0;

  std::size_t pos = m_absolute ? 1 : 0;
  while (pos < path.size()) {
    std::size_t next = path.find('/', pos);
    if (next == std::string::npos) {
      next = path.size();
    }
    join_(path.substr(pos, next - pos));
    pos = next + 1;
  }
}

Path::Path(const std::filesystem::path &path) : Path(path.string()) {}

void Path::parent_() {
  if (m_downwards > 0) {
    --m_downwards;
    if (m_downwards == 0) {
      if (m_absolute) {
        m_path = "/";
      } else {
        m_path = "";
      }
    } else {
      const auto pos = m_path.rfind('/');
      m_path = m_path.substr(0, pos);
    }
  } else if (!m_absolute) {
    ++m_upwards;
    m_path = "../" + m_path;
  } else {
    throw std::invalid_argument("absolute path violation");
  }
}

void Path::join_(const std::string &child) {
  if (child == ".") {
    return;
  }
  if (child == "..") {
    parent_();
  } else {
    if (m_downwards == 0) {
      m_path += child;
    } else {
      m_path += "/" + child;
    }
    ++m_downwards;
  }
}

bool Path::operator==(const Path &b) const noexcept {
  if (m_absolute != b.m_absolute) {
    return false;
  }
  if (!m_absolute && (m_upwards != b.m_upwards)) {
    return false;
  }
  return (m_downwards == b.m_downwards) && (m_path == b.m_path);
}

bool Path::operator!=(const Path &b) const noexcept {
  if (m_absolute != b.m_absolute) {
    return true;
  }
  if (!m_absolute && (m_upwards != b.m_upwards)) {
    return true;
  }
  return (m_downwards != b.m_downwards) || (m_path != b.m_path);
}

bool Path::operator<(const Path &b) const noexcept {
  // TODO could be improved
  return m_path < b.m_path;
}

bool Path::operator>(const Path &b) const noexcept {
  // TODO could be improved
  return m_path > b.m_path;
}

Path::operator std::string() const noexcept { return m_path; }

Path::operator std::filesystem::path() const noexcept { return m_path; }

Path::operator const std::string &() const noexcept { return m_path; }

const std::string &Path::string() const noexcept { return m_path; }

std::filesystem::path Path::path() const noexcept { return m_path; }

std::size_t Path::hash() const noexcept {
  return std::hash<std::string>{}(m_path);
}

bool Path::root() const noexcept {
  return (m_upwards == 0) && (m_downwards == 0);
}

bool Path::absolute() const noexcept { return m_absolute; }

bool Path::relative() const noexcept { return !m_absolute; }

bool Path::visible() const noexcept { return m_absolute || (m_upwards == 0); }

bool Path::escaping() const noexcept { return !m_absolute && (m_upwards > 0); }

bool Path::child_of(const Path &b) const { return b.parent_of(*this); }

bool Path::parent_of(const Path &b) const {
  if (m_absolute != b.m_absolute) {
    throw std::invalid_argument("cannot compare absolute and relative path");
  }
  // TODO we need to check upwards as well
  return (m_downwards + 1 == b.m_downwards) && (b.m_path.rfind(m_path, 0) == 0);
}

bool Path::ancestor_of(const Path &b) const { return b.descendant_of(*this); }

bool Path::descendant_of(const Path &b) const {
  if (m_absolute != b.m_absolute) {
    throw std::invalid_argument("cannot compare absolute and relative path");
  }
  // TODO we need to check upwards as well
  return (m_downwards < b.m_downwards) && (b.m_path.rfind(m_path, 0) == 0);
}

std::string Path::basename() const noexcept {
  const auto find = m_path.rfind('/');
  if (find == std::string::npos) {
    return m_path;
  }
  return m_path.substr(find + 1);
}

std::string Path::extension() const noexcept {
  auto bn = basename();
  const auto pos = bn.find('.');
  if (pos == std::string::npos) {
    return "";
  }
  return bn.substr(pos + 1);
}

Path Path::parent() const {
  Path result(*this);
  result.parent_();
  return result;
}

Path Path::join(const Path &b) const {
  if (b.m_absolute) {
    throw std::invalid_argument("cannot join an absolute path");
  }
  if (root()) {
    return Path("/" + b.m_path);
  }
  // TODO could be done directly
  return Path(m_path + "/" + b.m_path);
}

Path Path::rebase(const Path &on) const {
  if (!ancestor_of(on)) {
    throw std::invalid_argument("cannot rebase without ancestor");
  }
  const std::string result = m_path.substr(on.m_path.size() + 1);
  return Path(result);
}

std::ostream &operator<<(std::ostream &os, const Path &p) {
  return os << p.m_path;
}

} // namespace odr::internal::common
