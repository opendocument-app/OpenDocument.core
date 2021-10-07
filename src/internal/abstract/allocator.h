#ifndef ODR_INTERNAL_ABSTRACT_ALLOCATOR_H
#define ODR_INTERNAL_ABSTRACT_ALLOCATOR_H

#include <cstdint>
#include <functional>

namespace odr::internal::abstract {

using Allocator = std::function<void *(std::size_t size)>;

}

#endif // ODR_INTERNAL_ABSTRACT_ALLOCATOR_H
