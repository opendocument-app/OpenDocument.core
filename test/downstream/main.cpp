#include <odr/odr.hpp>

#include <iostream>

int main(void) {
  std::cout << "odr version=\"" << odr::get_version() << "\' "
            << "commit=\'" << odr::get_commit() << "\"" << std::endl;
}
