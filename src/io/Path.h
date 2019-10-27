#ifndef ODR_PATH_H
#define ODR_PATH_H

#include <cstdint>
#include <string>
#include <typeindex>
#include <iostream>

namespace odr {

class Path final {
public:
    Path() noexcept;
    Path(const char *);
    Path(const std::string &);
    Path(std::string &&);
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

    bool isAbsolute() const noexcept;
    bool isRelative() const noexcept;
    bool isVisible() const noexcept;
    bool isEscaping() const noexcept;
    bool childOf(const Path &) const noexcept;
    bool parentOf(const Path &) const noexcept;
    bool ancestorOf(const Path &) const noexcept;
    bool descendantOf(const Path &) const noexcept;

    std::string basename() const noexcept;
    std::string extension() const noexcept;
    std::string fullExtension() const noexcept;

    Path parent() const;
    Path join(const Path &) const noexcept;

private:
    std::string path_;
    std::uint32_t nesting_;

    friend struct ::std::hash<odr::Path>;
    friend std::ostream &operator<<(std::ostream &, const Path &);
};

}

namespace std {
template<>
struct hash<::odr::Path> {
    std::size_t operator()(const ::odr::Path &p) const { return p.hash(); }
};
}

#endif //ODR_PATH_H
