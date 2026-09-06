// Every public header, compiled as a C++20 consumer sees them.
//
// `odr` itself is built as C++23, and nothing propagates that: there is no
// `target_compile_features(odr PUBLIC …)` and no `cppstd` in the conan
// `package_info`, so a consumer keeps whatever standard it picked. This target
// is what keeps that true — a C++23 construct reaching `src/odr/*.hpp` breaks
// it here rather than in someone else's build.

#include <odr/archive.hpp>
#include <odr/definitions.hpp>
#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/font.hpp>
#include <odr/global_params.hpp>
#include <odr/html.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>
#include <odr/quantity.hpp>
#include <odr/style.hpp>
#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>
