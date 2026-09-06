#pragma once

#include <cstddef>
#include <functional>
#include <istream>
#include <memory>
#include <streambuf>
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

/// Holds what is written until `release`d, then passes it and everything after
/// straight through. `cap` bytes release it early.
class DeferredBuffer final : public std::streambuf {
public:
  /// @p release runs once, before the held bytes reach @p out: it writes the
  /// prologue they need in front of them.
  DeferredBuffer(std::ostream &out, std::size_t cap,
                 std::function<void()> release);

  void release();

protected:
  std::streamsize xsputn(const char *data, std::streamsize size) override;
  int overflow(int c) override;

private:
  std::ostream *m_out{nullptr};
  std::size_t m_cap{0};
  std::function<void()> m_release;
  std::string m_held;
  bool m_released{false};
};

class ViewStream : public std::istream {
public:
  explicit ViewStream(std::string_view view);

private:
  std::unique_ptr<std::streambuf> m_sbuf;
};

} // namespace odr::internal::util::stream
