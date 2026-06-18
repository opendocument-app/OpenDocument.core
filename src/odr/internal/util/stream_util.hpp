#pragma once

#include <istream>
#include <string>
#include <string_view>

namespace odr::internal::util::stream {

std::string read(std::istream &in);
std::string read(std::istream &in, std::size_t size);

void pipe(std::istream &in, std::ostream &out);

std::istream &pipe_line(std::istream &in, std::ostream &out, bool inclusive);
std::string read_line(std::istream &in, bool inclusive);

std::istream &pipe_until(std::istream &in, std::ostream &out, char until_char,
                         bool inclusive);
std::string read_until(std::istream &in, char until_char, bool inclusive);

class ViewStream : public std::istream {
public:
  explicit ViewStream(std::string_view view);

private:
  std::unique_ptr<std::streambuf> m_sbuf;
};

} // namespace odr::internal::util::stream
