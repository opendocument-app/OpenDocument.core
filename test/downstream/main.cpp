#include <odr/odr.hpp>

#include <iostream>

int main() {
  std::cout << "odr version=\"" << odr::version() << "\' "
            << "commit=\'" << odr::commit_hash() << "\"" << std::endl;
}
