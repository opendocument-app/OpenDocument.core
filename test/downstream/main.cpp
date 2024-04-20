#include <odr/open_document_reader.hpp>

#include <iostream>

int main(void) {
  std::cout << "odr version=\"" << odr::OpenDocumentReader::version() << "\' "
            << "commit=\'" << odr::OpenDocumentReader::commit() << "\""
            << std::endl;
}
