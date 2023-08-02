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

  for (auto it = Iterator(path, m_absolute ? 1 : 0); it != end(); ++it) {
    join_(*it);
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
    if (m_upwards + m_downwards == 0) {
      m_path = "..";
    } else {
      m_path = "../" + m_path;
    }
    ++m_upwards;
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
    if (m_upwards + m_downwards == 0) {
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

Path Path::rebase(const Path &b) const {
  Path result = m_absolute ? "/" : "";
  auto common_root = this->common_root(b);

  Path sub_a;
  {
    auto it_a = begin();
    for (std::uint32_t i = 0;
         i < common_root.m_upwards + common_root.m_downwards; ++i) {
      ++it_a;
    }
    for (; it_a != end(); ++it_a) {
      sub_a.join_(*it_a);
    }
  }

  Path sub_b;
  {
    auto it_b = b.begin();
    for (std::uint32_t i = 0;
         i < common_root.m_upwards + common_root.m_downwards; ++i) {
      ++it_b;
    }
    for (; it_b != b.end(); ++it_b) {
      sub_b.join_(*it_b);
    }
  }

  for (std::uint32_t i = 0; i < sub_b.m_downwards; ++i) {
    result.join_("..");
  }

  for (auto part : sub_a) {
    result.join_(part);
  }

  return result;
}

Path Path::common_root(const Path &b) const {
  if (m_absolute != b.m_absolute) {
    throw std::invalid_argument(
        "no common root between absolute and relative path");
  }
  if (b.m_upwards > m_upwards) {
    throw std::invalid_argument("parent directories unknown");
  }

  Path result = m_absolute ? "/" : "";

  auto it_a = begin();
  auto it_b = b.begin();
  while (true) {
    if ((it_a == end()) || (it_b == b.end()) || (*it_a != *it_b)) {
      break;
    }
    result.join_(*it_a);
    ++it_a;
    ++it_b;
  }

  return result;
}

Path::Iterator Path::begin() const { return Iterator(*this); }

Path::Iterator Path::end() const { return Iterator(); }

Path::Iterator::Iterator() : m_path{nullptr}, m_begin{std::string::npos} {}

Path::Iterator::Iterator(const std::string &path) : Iterator(path, 0) {}

Path::Iterator::Iterator(const std::string &path, const std::size_t begin)
    : m_path{&path}, m_begin{begin} {
  fill_();
}

Path::Iterator::Iterator(const Path &path)
    : Iterator(path.m_path, path.m_absolute ? 1 : 0) {}

void Path::Iterator::fill_() {
  if (m_begin >= m_path->size()) {
    m_begin = std::string::npos;
  }

  if (m_begin == std::string::npos) {
    m_part = "";
    return;
  }

  auto part_end = m_path->find('/', m_begin);
  if (part_end == std::string::npos) {
    part_end = m_path->size();
  }
  m_part = m_path->substr(m_begin, part_end - m_begin);
}

Path::Iterator::reference Path::Iterator::operator*() const { return m_part; }

Path::Iterator::pointer Path::Iterator::operator->() const { return &m_part; }

bool Path::Iterator::operator==(const Iterator &other) const {
  return m_begin == other.m_begin;
}

bool Path::Iterator::operator!=(const Iterator &other) const {
  return m_begin != other.m_begin;
}

Path::Iterator &Path::Iterator::operator++() {
  m_begin = m_path->find('/', m_begin);
  if (m_begin != std::string::npos) {
    ++m_begin;
  }
  fill_();
  return *this;
}

Path::Iterator Path::Iterator::operator++(int) {
  auto old = *this;
  this->operator++();
  return old;
}

std::ostream &operator<<(std::ostream &os, const Path &p) {
  return os << p.m_path;
}

} // namespace odr::internal::common
