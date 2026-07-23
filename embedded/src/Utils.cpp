#include <time.h>
#include "Utils.h"

const char* getFormattedTimestamp() {
    static char timestamp[20];
    

    time_t now = time(nullptr);
    if (now < 0) {
        return nullptr;
    }

    struct tm timeinfo;
    if (localtime_r(&now, &timeinfo) == nullptr) {
        return nullptr;
    }
    
    if (timeinfo.tm_year < 120) { 
        return nullptr; 
    }
    
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return timestamp;
}