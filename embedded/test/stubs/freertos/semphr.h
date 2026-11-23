#pragma once
#include "FreeRTOS.h"
typedef void *SemaphoreHandle_t;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return reinterpret_cast<SemaphoreHandle_t>(1); }
inline void vSemaphoreDelete(SemaphoreHandle_t) {}
inline int xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return 1; }
inline int xSemaphoreGive(SemaphoreHandle_t) { return 1; }
