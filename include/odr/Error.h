#ifndef ODR_ERROR_H
#define ODR_ERROR_H

namespace odr {

enum class Error {
    NONE,
    FILE_NOT_FOUND,
    FILE_NOT_SUPPORTED,
    INVALID_PASSWORD
};

}

#endif //ODR_ERROR_H
