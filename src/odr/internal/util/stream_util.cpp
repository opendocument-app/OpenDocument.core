#include <odr/internal/util/stream_util.hpp>

#include <array>
#include <iterator>
#include <sstream>
#include <streambuf>

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
  result.resize(static_cast<std::size_t>(in.gcount()));
  return result;
}

void stream::pipe(std::istream &in, std::ostream &out) {
  static constexpr std::size_t BUFFER_SIZE = 4096;

  std::array<char, BUFFER_SIZE> buffer{};

  while (true) {
    in.read(buffer.data(), BUFFER_SIZE);
    const auto read = in.gcount();
    if (read == 0) {
      break;
    }
    out.write(buffer.data(), read);
  }
}

// from https://stackoverflow.com/a/6089413
std::istream &stream::pipe_line(std::istream &in, std::ostream &out,
                                const bool inclusive) {
  // reading through the streambuf is faster than through the istream, but has
  // to be guarded by a sentry
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

namespace odr::internal::util::stream {

namespace {

/// Read-only stream buffer over an existing `string_view`, which must outlive
/// it.
class ViewStreamBuf : public std::streambuf {
public:
  explicit ViewStreamBuf(std::string_view view) {
    // the get area is never written through
    auto *begin = const_cast<char *>(view.data());
    setg(begin, begin, begin + view.size());
  }

protected:
  pos_type seekoff(const off_type off, const std::ios_base::seekdir dir,
                   const std::ios_base::openmode which) override {
    if ((which & std::ios_base::in) == 0) {
      return pos_type(off_type(-1));
    }

    off_type position = off;
    if (dir == std::ios_base::cur) {
      position += gptr() - eback();
    } else if (dir == std::ios_base::end) {
      position += egptr() - eback();
    } else if (dir != std::ios_base::beg) {
      return pos_type(off_type(-1));
    }

    if (position < 0 || position > egptr() - eback()) {
      return pos_type(off_type(-1));
    }
    setg(eback(), eback() + position, egptr());
    return position;
  }

  pos_type seekpos(const pos_type pos,
                   const std::ios_base::openmode which) override {
    return seekoff(pos, std::ios_base::beg, which);
  }
};

} // namespace

DeferredBuffer::DeferredBuffer(std::ostream &out, const std::size_t cap,
                               std::function<void()> release)
    : m_out{&out}, m_cap{cap}, m_release{std::move(release)} {}

void DeferredBuffer::release() {
  if (m_released) {
    return;
  }
  m_released = true;
  m_release();
  m_out->write(m_held.data(), static_cast<std::streamsize>(m_held.size()));
  m_held.clear();
  m_held.shrink_to_fit();
}

std::streamsize DeferredBuffer::xsputn(const char *data,
                                       const std::streamsize size) {
  if (m_released) {
    m_out->write(data, size);
    return size;
  }
  m_held.append(data, static_cast<std::size_t>(size));
  if (m_held.size() > m_cap) {
    release();
  }
  return size;
}

int DeferredBuffer::overflow(const int c) {
  if (c == traits_type::eof()) {
    return traits_type::not_eof(c);
  }
  const char value = traits_type::to_char_type(c);
  xsputn(&value, 1);
  return c;
}

ViewStream::ViewStream(std::string_view view)
    : std::istream(nullptr), m_sbuf{std::make_unique<ViewStreamBuf>(view)} {
  rdbuf(m_sbuf.get());
}

} // namespace odr::internal::util::stream
