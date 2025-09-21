#pragma once

#include <ostream>
#include <streambuf>

namespace odr::internal {

class NullBuffer final : public std::streambuf {
public:
  int overflow(const int c) override { return c; }
};

class NullStream final : public std::ostream {
public:
  NullStream() : std::ostream(&m_nb) {}

private:
  NullBuffer m_nb;
};

} // namespace odr::internal
