#include <odr/internal/util/stream_util.hpp>

#include <iostream>
#include <iterator>

namespace odr::internal::util {

std::string stream::read(std::istream &in) {
  return std::string{std::istreambuf_iterator<char>(in), {}};
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
std::istream &stream::getline(std::istream &in, std::ostream &out) {
  // The characters in the stream are read one-by-one using a std::streambuf.
  // That is faster than reading them one-by-one using the std::istream.
  // Code that uses streambuf this way must be guarded by a sentry object.
  // The sentry object performs various tasks,
  // such as thread synchronization and updating the stream state.

  std::istream::sentry se(in, true);
  std::streambuf *sb = in.rdbuf();

  while (true) {
    int c = sb->sbumpc();
    switch (c) {
    case '\n':
      return in;
    case '\r':
      if (sb->sgetc() == '\n') {
        sb->sbumpc();
      }
      return in;
    case std::streambuf::traits_type::eof():
      in.setstate(std::ios::eofbit);
      return in;
    default:
      out.put((char)c);
    }
  }
}

} // namespace odr::internal::util
