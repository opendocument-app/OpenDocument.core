#pragma once

#include <ostream>

namespace odr::internal::common {

class NullBuffer final : public std::streambuf {
public:
  int overflow(int c) final { return c; }
};

class NullStream final : public std::ostream {
public:
  NullStream() : std::ostream(&m_nb) {}

private:
  NullBuffer m_nb;
};

} // namespace odr::internal::common
