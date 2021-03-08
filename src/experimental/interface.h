#ifndef ODR_EXPERIMENTAL_INTERFACE_H
#define ODR_EXPERIMENTAL_INTERFACE_H

namespace odr {
struct FileMeta;
}

namespace odr::experimental {
struct FileMeta;

odr::FileMeta convert(const FileMeta &meta);
} // namespace odr::experimental

#endif // ODR_EXPERIMENTAL_INTERFACE_H
