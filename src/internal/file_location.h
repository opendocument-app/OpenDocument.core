#ifndef ODR_INTERNAL_FILE_LOCATION_H
#define ODR_INTERNAL_FILE_LOCATION_H

namespace odr::internal {

enum class FileLocation {
  UNKNOWN,
  MEMORY,
  DISC,
  NETWORK,
};

}

#endif // ODR_INTERNAL_FILE_LOCATION_H
