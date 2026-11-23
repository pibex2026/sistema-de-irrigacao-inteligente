#pragma once
#include <cstdint>

inline int64_t g_mockMillis = 1000;

namespace TimeUtils {
inline int64_t getMillis() { return g_mockMillis; }
} // namespace TimeUtils
