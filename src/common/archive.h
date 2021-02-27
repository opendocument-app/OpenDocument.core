#ifndef ODR_COMMON_ARCHIVE_H
#define ODR_COMMON_ARCHIVE_H

#include <abstract/archive.h>

namespace odr::common {

template<typename Impl>
class Archive : public abstract::Archive {
public:


private:
  Impl m_impl;
};

}

#endif // ODR_COMMON_ARCHIVE_H
