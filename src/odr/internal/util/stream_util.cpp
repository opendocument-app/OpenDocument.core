#include <odr/internal/util/stream_util.hpp>

#include <iostream>
#include <iterator>
#include <sstream>

namespace odr::internal::util {

using char_type = std::streambuf::char_type;
using int_type = std::streambuf::int_type;
static constexpr int_type eof = std::streambuf::traits_type::eof();

std::string stream::read(std::istream &in) {
  return {std::istreambuf_iterator(in), {}};
}

std::string stream::read(std::istream &in, const std::size_t size) {
  std::string result(size, '\0');
  in.read(result.data(), static_cast<std::streamsize>(size));
  result.resize(in.gcount());
  return result;
}

void stream::pipe(std::istream &in, std::ostream &out) {
  static constexpr auto BUFFER_SIZE = 4096;

  char buffer[BUFFER_SIZE];

  while (true) {
    in.read(buffer, BUFFER_SIZE);
    const auto read = in.gcount();
    if (read == 0) {
      break;
    }
    out.write(buffer, read);
  }
}

// from https://stackoverflow.com/a/6089413
std::istream &stream::pipe_line(std::istream &in, std::ostream &out,
                                const bool inclusive) {
  // The characters in the stream are read one-by-one using a std::streambuf.
  // That is faster than reading them one-by-one using the std::istream.
  // Code that uses streambuf this way must be guarded by a sentry object.
  // The sentry object performs various tasks, such as thread synchronization
  // and updating the stream state.

  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  while (true) {
    switch (int_type c = sb->sbumpc()) {
    case '\n':
      if (inclusive) {
        out.put(static_cast<char_type>(c));
      }
      return in;
    case '\r':
      if (inclusive) {
        out.put(static_cast<char_type>(c));
      }
      if (sb->sgetc() == '\n') {
        c = sb->sbumpc();
        if (inclusive) {
          out.put(static_cast<char_type>(c));
        }
      }
      return in;
    case eof:
      in.setstate(std::ios::eofbit);
      return in;
    default:
      out.put(static_cast<char_type>(c));
    }
  }
}

std::string stream::read_line(std::istream &in, const bool inclusive) {
  std::stringstream ss;
  pipe_line(in, ss, inclusive);
  return ss.str();
}

std::istream &stream::pipe_until(std::istream &in, std::ostream &out,
                                 const char until_char, const bool inclusive) {
  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  while (true) {
    const int_type c = sb->sbumpc();
    if (c == eof) {
      in.setstate(std::ios::eofbit);
      return in;
    }
    if (inclusive) {
      out.put(static_cast<char>(c));
    }
    if (c == until_char) {
      return in;
    }
    if (!inclusive) {
      out.put(static_cast<char>(c));
    }
  }
}

std::string stream::read_until(std::istream &in, const char until_char,
                               const bool inclusive) {
  std::stringstream ss;
  pipe_until(in, ss, until_char, inclusive);
  return ss.str();
}

} // namespace odr::internal::util
