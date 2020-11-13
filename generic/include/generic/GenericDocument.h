#ifndef ODR_GENERIC_GENERICDOCUMENT_H
#define ODR_GENERIC_GENERICDOCUMENT_H

#include <memory>

namespace odr::generic {

class GenericElement {
public:
  GenericElement first_child();
  GenericElement next_sibling();

private:
};

class GenericDocument {
public:
};

class GenericText {
public:
};

class GenericPresentation {
public:
};

class GenericSpreadsheet {
public:
};

}

#endif // ODR_GENERIC_GENERICDOCUMENT_H
