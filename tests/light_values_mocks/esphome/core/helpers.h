#pragma once
// Only platform dependencies are adapted. LightColorValues, color modes and
// their conversion math are read unchanged from the installed ESPHome version.
#include <algorithm>
#include <cstdint>
#ifdef _MSC_VER
#define __builtin_is_constant_evaluated() true
#endif
