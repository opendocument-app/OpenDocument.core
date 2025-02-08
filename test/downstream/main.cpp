#include <odr/odr.hpp>

#include <iostream>

int main(void) {
  std::cout << "odr version=\"" << odr::OpenDocumentReader::version() << "\' "
            << "commit=\'" << odr::OpenDocumentReader::commit() << "\""
            << std::endl;
}
