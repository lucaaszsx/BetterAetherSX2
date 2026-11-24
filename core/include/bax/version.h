#pragma once

#include <string_view>

namespace bax {

// Version of the native core, set from the CMake project version so the app and
// bug reports name the exact build.
std::string_view coreVersion();

} // namespace bax
