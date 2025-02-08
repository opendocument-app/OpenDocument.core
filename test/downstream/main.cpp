#include <odr/odr.hpp>

#include <iostream>

int main(void) {
  std::cout << "odr version=\"" << odr::version() << "\' "
            << "commit=\'" << odr::commit() << "\"" << std::endl;
}
