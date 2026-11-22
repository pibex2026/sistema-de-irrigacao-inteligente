#include "TimeUtils.hpp"
#include "esp_timer.h"
#include <cstdio>
#include <ctime>

namespace TimeUtils {

bool isNtpSynced() {
  time_t now = time(nullptr);
  if (now < 0) return false;
  struct tm timeinfo {};
  if (localtime_r(&now, &timeinfo) == nullptr) return false;
  return timeinfo.tm_year >= 120; // >= 2020
}

void getFormattedTimestamp(char *outBuf, size_t bufLen) {
  if (outBuf == nullptr || bufLen == 0) return;

  time_t now = time(nullptr);
  struct tm timeinfo {};
  if (now >= 0 && localtime_r(&now, &timeinfo) != nullptr && timeinfo.tm_year >= 120) {
    std::strftime(outBuf, bufLen, "%Y-%m-%d %H:%M:%S", &timeinfo);
    return;
  }

  // Fallback deterministico offline: UPTIME_<segundos>s
  std::snprintf(outBuf, bufLen, "UPTIME_%lus", static_cast<unsigned long>(getUptimeSec()));
}

uint32_t getUptimeSec() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000000LL);
}

int64_t getMillis() {
  return esp_timer_get_time() / 1000LL;
}

} // namespace TimeUtils
