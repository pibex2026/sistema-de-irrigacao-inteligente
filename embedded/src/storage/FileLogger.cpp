#include "FileLogger.hpp"
#include "config.hpp"
#include "esp_littlefs.h"
#include "esp_log.h"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

static const char *TAG = "FileLogger";

static const char *READINGS_HEADER = "temperatura_c,umidade,umidade_solo,adc_bruto,valido,timestamp\n";
static const char *INFERENCE_HEADER =
    "temperatura_c,umidade,umidade_solo,soil_id,crop_id,stage_id,"
    "p_off,p_irrigate,p_stress,decisao,valvula_aberta,fallback,timestamp\n";

FileLogger::FileLogger() {
  m_mutex = xSemaphoreCreateMutex();
}

FileLogger::~FileLogger() {
  m_running = false;
  if (m_queue != nullptr && m_taskHandle != nullptr) {
    LogQueueItem stopItem{};
    stopItem.kind = LogQueueItem::Kind::STOP;
    xQueueSend(m_queue, &stopItem, pdMS_TO_TICKS(50));
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  if (m_queue != nullptr) {
    vQueueDelete(m_queue);
    m_queue = nullptr;
  }
  if (m_mutex != nullptr) {
    vSemaphoreDelete(m_mutex);
    m_mutex = nullptr;
  }
}

esp_err_t FileLogger::init() {
  esp_vfs_littlefs_conf_t conf{};
  conf.base_path = Config::Storage::LITTLEFS_MOUNT_POINT;
  conf.partition_label = Config::Storage::LITTLEFS_PARTITION_LABEL;
  conf.format_if_mount_failed = true;
  conf.dont_mount = false;

  esp_err_t err = esp_vfs_littlefs_register(&conf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Falha ao montar LittleFS: %s", esp_err_to_name(err));
    return err;
  }

  size_t total = 0, used = 0;
  if (esp_littlefs_info(conf.partition_label, &total, &used) == ESP_OK) {
    ESP_LOGI(TAG, "LittleFS montado: %zu / %zu bytes usados.", used, total);
  }

  m_mounted = true;
  ensureCsvHeader(Config::Storage::READINGS_CSV_PATH, READINGS_HEADER);
  ensureCsvHeader(Config::Storage::INFERENCE_CSV_PATH, INFERENCE_HEADER);

  if (m_queue == nullptr) {
    m_queue = xQueueCreate(Config::Tasks::LOGGER_QUEUE_LEN, sizeof(LogQueueItem));
  }
  if (m_queue != nullptr && m_taskHandle == nullptr) {
    m_running = true;
    const BaseType_t rc = xTaskCreatePinnedToCore(
        loggerTask,
        "logger_task",
        Config::Tasks::LOGGER_STACK,
        this,
        Config::Tasks::LOGGER_PRIO,
        &m_taskHandle,
        tskNO_AFFINITY);
    if (rc != pdPASS) {
      m_running = false;
      m_taskHandle = nullptr;
      ESP_LOGW(TAG, "Falha ao criar logger_task assincrona.");
    }
  }
  return ESP_OK;
}

bool FileLogger::ensureCsvHeader(const char *path, const char *header) const {
  struct stat st{};
  if (stat(path, &st) == 0 && st.st_size > 0) {
    return true;
  }
  FILE *f = std::fopen(path, "w");
  if (f == nullptr) return false;
  std::fputs(header, f);
  std::fclose(f);
  return true;
}

void FileLogger::rotateIfNeeded(const char *path, const char *prevPath,
                                size_t maxBytes, const char *header) const {
  struct stat st{};
  if (stat(path, &st) != 0) {
    ensureCsvHeader(path, header);
    return;
  }
  if (static_cast<size_t>(st.st_size) < maxBytes) {
    return;
  }

  std::remove(prevPath);
  std::rename(path, prevPath);
  ensureCsvHeader(path, header);
  ESP_LOGW(TAG, "Rotacao de log executada: %s -> %s", path, prevPath);
}

uint32_t FileLogger::getQueuePending() const {
  if (m_queue == nullptr) return 0;
  return static_cast<uint32_t>(uxQueueMessagesWaiting(m_queue));
}

bool FileLogger::enqueueItem(const LogQueueItem &item) {
  if (!m_mounted || m_queue == nullptr) return false;
  if (xQueueSend(m_queue, &item, 0) == pdTRUE) {
    return true;
  }

  // Fila cheia: descarta o registro mais antigo para nao bloquear sensores/IA
  LogQueueItem discarded{};
  if (xQueueReceive(m_queue, &discarded, 0) == pdTRUE) {
    m_droppedCount.fetch_add(1);
  }
  if (xQueueSend(m_queue, &item, 0) == pdTRUE) {
    return true;
  }
  m_droppedCount.fetch_add(1);
  return false;
}

bool FileLogger::enqueueSensorData(const SensorData &data) {
  LogQueueItem item{};
  item.kind = LogQueueItem::Kind::SENSOR;
  item.sensor = data;
  return enqueueItem(item);
}

bool FileLogger::enqueueInferenceData(const InferenceLogEntry &entry) {
  LogQueueItem item{};
  item.kind = LogQueueItem::Kind::INFERENCE;
  item.inference = entry;
  return enqueueItem(item);
}

bool FileLogger::logSensorData(const SensorData &data) {
  return enqueueSensorData(data);
}

bool FileLogger::logInferenceData(const InferenceLogEntry &entry) {
  return enqueueInferenceData(entry);
}

void FileLogger::loggerTask(void *arg) {
  auto *self = static_cast<FileLogger *>(arg);
  LogQueueItem item{};

  while (self->m_running.load()) {
    if (xQueueReceive(self->m_queue, &item, pdMS_TO_TICKS(1000)) == pdTRUE) {
      if (item.kind == LogQueueItem::Kind::STOP) {
        break;
      }
      if (item.kind == LogQueueItem::Kind::SENSOR) {
        self->writeSensorDataSync(item.sensor);
      } else if (item.kind == LogQueueItem::Kind::INFERENCE) {
        self->writeInferenceDataSync(item.inference);
      }
    }
    self->m_stackFreeBytes.store(
        static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t)));
  }

  self->m_taskHandle = nullptr;
  vTaskDelete(nullptr);
}

bool FileLogger::writeSensorDataSync(const SensorData &data) {
  if (!m_mounted || m_mutex == nullptr) return false;
  xSemaphoreTake(m_mutex, portMAX_DELAY);

  rotateIfNeeded(Config::Storage::READINGS_CSV_PATH, Config::Storage::READINGS_PREV_PATH,
                 Config::Storage::MAX_READINGS_LOG_BYTES, READINGS_HEADER);

  FILE *f = std::fopen(Config::Storage::READINGS_CSV_PATH, "a");
  bool ok = false;
  if (f != nullptr) {
    std::fprintf(f, "%.1f,%u,%u,%u,%u,%s\n",
                 data.getTemperatureC(),
                 static_cast<unsigned>(data.humidity),
                 static_cast<unsigned>(data.soilMoisture),
                 static_cast<unsigned>(data.rawSoilAdc),
                 data.valid ? 1U : 0U,
                 data.timestamp);
    std::fclose(f);
    ok = true;
  }

  xSemaphoreGive(m_mutex);
  return ok;
}

bool FileLogger::writeInferenceDataSync(const InferenceLogEntry &entry) {
  if (!m_mounted || m_mutex == nullptr) return false;
  xSemaphoreTake(m_mutex, portMAX_DELAY);

  rotateIfNeeded(Config::Storage::INFERENCE_CSV_PATH, Config::Storage::INFERENCE_PREV_PATH,
                 Config::Storage::MAX_INFERENCE_LOG_BYTES, INFERENCE_HEADER);

  FILE *f = std::fopen(Config::Storage::INFERENCE_CSV_PATH, "a");
  bool ok = false;
  if (f != nullptr) {
    std::fprintf(f, "%.1f,%u,%u,%u,%u,%u,%.4f,%.4f,%.4f,%u,%u,%u,%s\n",
                 entry.sensors.getTemperatureC(),
                 static_cast<unsigned>(entry.sensors.humidity),
                 static_cast<unsigned>(entry.sensors.soilMoisture),
                 static_cast<unsigned>(entry.context.soilType),
                 static_cast<unsigned>(entry.context.cropId),
                 static_cast<unsigned>(entry.context.seedlingStage),
                 entry.inference.probabilities[0],
                 entry.inference.probabilities[1],
                 entry.inference.probabilities[2],
                 static_cast<unsigned>(entry.inference.decision),
                 entry.valveOpen ? 1U : 0U,
                 entry.inference.fallbackTriggered ? 1U : 0U,
                 entry.inference.timestamp);
    std::fclose(f);
    ok = true;
  }

  xSemaphoreGive(m_mutex);
  return ok;
}

std::vector<SensorData> FileLogger::readRecentReadings(size_t maxCount) const {
  std::vector<SensorData> ring;
  if (!m_mounted || maxCount == 0 || m_mutex == nullptr) return ring;

  xSemaphoreTake(m_mutex, portMAX_DELAY);
  FILE *f = std::fopen(Config::Storage::READINGS_CSV_PATH, "r");
  if (f != nullptr) {
    ring.resize(maxCount);
    size_t count = 0;
    char line[160];
    bool firstLine = true;
    while (std::fgets(line, sizeof(line), f) != nullptr) {
      if (firstLine) {
        firstLine = false;
        if (std::strncmp(line, "temperatura", 11) == 0) continue;
      }
      float tempC = 0.0f;
      unsigned hum = 0, soil = 0, adc = 0, valid = 0;
      char ts[24] = "";
      if (std::sscanf(line, "%f,%u,%u,%u,%u,%23[^\r\n]", &tempC, &hum, &soil, &adc, &valid, ts) >= 5) {
        SensorData d{};
        d.temperatureX10 = static_cast<int16_t>(tempC * 10.0f);
        d.humidity = static_cast<uint8_t>(hum);
        d.soilMoisture = static_cast<uint8_t>(soil);
        d.rawSoilAdc = static_cast<uint16_t>(adc);
        d.valid = (valid != 0);
        std::strncpy(d.timestamp, ts, sizeof(d.timestamp) - 1);
        ring[count % maxCount] = d;
        count++;
      }
    }
    std::fclose(f);

    const size_t n = (count < maxCount) ? count : maxCount;
    std::vector<SensorData> out;
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      const size_t idx = (count - 1 - i) % maxCount;
      out.push_back(ring[idx]);
    }
    xSemaphoreGive(m_mutex);
    return out;
  }
  xSemaphoreGive(m_mutex);
  return {};
}

std::vector<InferenceLogEntry> FileLogger::readRecentInferences(size_t maxCount) const {
  std::vector<InferenceLogEntry> ring;
  if (!m_mounted || maxCount == 0 || m_mutex == nullptr) return ring;

  xSemaphoreTake(m_mutex, portMAX_DELAY);
  FILE *f = std::fopen(Config::Storage::INFERENCE_CSV_PATH, "r");
  if (f != nullptr) {
    ring.resize(maxCount);
    size_t count = 0;
    char line[220];
    bool firstLine = true;
    while (std::fgets(line, sizeof(line), f) != nullptr) {
      if (firstLine) {
        firstLine = false;
        if (std::strncmp(line, "temperatura", 11) == 0) continue;
      }
      float tempC = 0.0f, p0 = 0.0f, p1 = 0.0f, p2 = 0.0f;
      unsigned hum = 0, soil = 0, sId = 0, cId = 0, stId = 0, dec = 0, valve = 0, fb = 0;
      char ts[24] = "";
      if (std::sscanf(line, "%f,%u,%u,%u,%u,%u,%f,%f,%f,%u,%u,%u,%23[^\r\n]",
                      &tempC, &hum, &soil, &sId, &cId, &stId, &p0, &p1, &p2, &dec, &valve, &fb, ts) >= 12) {
        InferenceLogEntry e{};
        e.sensors.temperatureX10 = static_cast<int16_t>(tempC * 10.0f);
        e.sensors.humidity = static_cast<uint8_t>(hum);
        e.sensors.soilMoisture = static_cast<uint8_t>(soil);
        e.sensors.valid = true;
        e.context.soilType = static_cast<uint8_t>(sId);
        e.context.cropId = static_cast<uint8_t>(cId);
        e.context.seedlingStage = static_cast<uint8_t>(stId);
        e.inference.probabilities[0] = p0;
        e.inference.probabilities[1] = p1;
        e.inference.probabilities[2] = p2;
        e.inference.decision = static_cast<IrrigationDecision>(dec);
        e.inference.fallbackTriggered = (fb != 0);
        e.inference.valid = true;
        e.valveOpen = (valve != 0);
        std::strncpy(e.inference.timestamp, ts, sizeof(e.inference.timestamp) - 1);
        ring[count % maxCount] = e;
        count++;
      }
    }
    std::fclose(f);

    const size_t n = (count < maxCount) ? count : maxCount;
    std::vector<InferenceLogEntry> out;
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
      const size_t idx = (count - 1 - i) % maxCount;
      out.push_back(ring[idx]);
    }
    xSemaphoreGive(m_mutex);
    return out;
  }
  xSemaphoreGive(m_mutex);
  return {};
}

StorageStats FileLogger::getStorageStats() const {
  StorageStats stats{};
  stats.mounted = m_mounted;
  if (!m_mounted || m_mutex == nullptr) return stats;

  xSemaphoreTake(m_mutex, portMAX_DELAY);
  size_t total = 0, used = 0;
  if (esp_littlefs_info(Config::Storage::LITTLEFS_PARTITION_LABEL, &total, &used) == ESP_OK) {
    stats.totalBytes = static_cast<uint32_t>(total);
    stats.usedBytes = static_cast<uint32_t>(used);
  }

  auto fileSize = [](const char *path) -> uint32_t {
    struct stat st{};
    return (stat(path, &st) == 0) ? static_cast<uint32_t>(st.st_size) : 0;
  };
  stats.readingsBytes = fileSize(Config::Storage::READINGS_CSV_PATH);
  stats.readingsPrevBytes = fileSize(Config::Storage::READINGS_PREV_PATH);
  stats.inferenceBytes = fileSize(Config::Storage::INFERENCE_CSV_PATH);
  stats.inferencePrevBytes = fileSize(Config::Storage::INFERENCE_PREV_PATH);
  xSemaphoreGive(m_mutex);
  return stats;
}
