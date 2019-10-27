#include "Path.h"
#include <algorithm>
#include <stdexcept>

namespace odr {

Path::Path() noexcept : Path("") {}

Path::Path(const char *path) : Path(std::string(path)) {}

Path::Path(const std::string &path) : Path(std::string(path)) {}

Path::Path(std::string &&path) :
        path_(std::move(path)) {
    // TODO normalize ../
    // TODO remove ./
    // TODO remove forward slash
    // TODO throw on illegal chars
    if (path_.rfind("./", 0) == 0) {
        path_ = path_.substr(2);
    }

    nesting_ = std::count(path_.begin(), path_.end(), '/');
    if (nesting_ == 0) nesting_ = 1;
}

bool Path::operator==(const Path &b) const noexcept {
    return (nesting_ == b.nesting_) && (path_ == b.path_);
}

bool Path::operator!=(const Path &b) const noexcept {
    return (nesting_ != b.nesting_) || (path_ != b.path_);
}

bool Path::operator<(const odr::Path &b) const noexcept {
    return (nesting_ < b.nesting_) || (path_ < b.path_);
}

bool Path::operator>(const odr::Path &b) const noexcept {
    return (nesting_ > b.nesting_) || (path_ > b.path_);
}

Path::operator std::string() const noexcept {
    return path_;
}

Path::operator const std::string &() const noexcept {
    return path_;
}

const std::string &Path::string() const noexcept {
    return path_;
}

std::size_t Path::hash() const noexcept {
    static const std::hash<std::string> stringHash;
    return stringHash(path_);
}

bool Path::isAbsolute() const noexcept {
    return path_.rfind('/', 0) == 0;
}

bool Path::isRelative() const noexcept {
    return !isAbsolute();
}

bool Path::isVisible() const noexcept {
    return true; // TODO
}

bool Path::isEscaping() const noexcept {
    return false; // TODO
}

bool Path::childOf(const Path &b) const noexcept {
    return b.parentOf(*this);
}

bool Path::parentOf(const Path &b) const noexcept {
    return (nesting_ + 1 == b.nesting_) && (b.path_.rfind(path_, 0) == 0);
}

bool Path::ancestorOf(const Path &b) const noexcept {
    return b.descendantOf(*this);
}

bool Path::descendantOf(const Path &b) const noexcept {
    return (nesting_ < b.nesting_) && (b.path_.rfind(path_, 0) == 0);
}

std::string Path::basename() const noexcept {
    const auto find = path_.rfind('/');
    if (find == std::string::npos) return path_;
    return path_.substr(find + 1);
}

std::string Path::extension() const noexcept {
    return {}; // TODO
}

std::string Path::fullExtension() const noexcept {
    return {}; // TODO
}

Path Path::parent() const {
    const auto find = path_.rfind('/');
    if (find == std::string::npos) throw std::invalid_argument("this");
    return path_.substr(0, find);
}

Path Path::join(const Path &b) const noexcept {
    const std::string result = path_ + "/" + b.path_;
    return Path(result);
}

std::ostream &operator<<(std::ostream &os, const odr::Path &p) {
    return os << p.path_;
}

}
