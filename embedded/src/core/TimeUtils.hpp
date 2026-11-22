#pragma once

#include <cstddef>
#include <cstdint>

namespace TimeUtils {

bool isNtpSynced();
void getFormattedTimestamp(char *outBuf, size_t bufLen);
uint32_t getUptimeSec();
int64_t getMillis();

} // namespace TimeUtils
