#pragma once

#include <clocale>
#include <locale>
#include <string>

namespace odr::test {

/// A decimal comma and grouped thousands, independent of installed locales.
class GroupedNumbers final : public std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
  char do_thousands_sep() const override { return '.'; }
  std::string do_grouping() const override { return "\3"; }
};

/// Restores C and C++ locales, including after a failed assertion or skip.
class LocaleGuard final {
public:
  LocaleGuard() : m_c(std::setlocale(LC_ALL, nullptr)) {}
  ~LocaleGuard() {
    std::locale::global(m_cpp);
    std::setlocale(LC_ALL, m_c.c_str());
  }

  LocaleGuard(const LocaleGuard &) = delete;
  LocaleGuard &operator=(const LocaleGuard &) = delete;

private:
  const std::locale m_cpp;
  const std::string m_c;
};

} // namespace odr::test
