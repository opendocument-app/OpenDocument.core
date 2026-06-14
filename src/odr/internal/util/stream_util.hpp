#pragma once

#include <iosfwd>
#include <streambuf>
#include <string>
#include <string_view>

namespace odr::internal::util::stream {

/// Read-only stream buffer over an existing `string_view`, so a `std::istream`
/// can scan it without copying into a `std::string`/`std::istringstream`. The
/// view must outlive the buffer. Only the get area is exposed; seeking is not
/// supported.
class ViewStreamBuf : public std::streambuf {
public:
  explicit ViewStreamBuf(std::string_view view) {
    // The get area is only ever read, never written through, so dropping the
    // `const` is safe.
    char *begin = const_cast<char *>(view.data());
    setg(begin, begin, begin + view.size());
  }
};

std::string read(std::istream &in);
std::string read(std::istream &in, std::size_t size);

void pipe(std::istream &in, std::ostream &out);

std::istream &pipe_line(std::istream &in, std::ostream &out, bool inclusive);
std::string read_line(std::istream &in, bool inclusive);

std::istream &pipe_until(std::istream &in, std::ostream &out, char until_char,
                         bool inclusive);
std::string read_until(std::istream &in, char until_char, bool inclusive);

} // namespace odr::internal::util::stream
