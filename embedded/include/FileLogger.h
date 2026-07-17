#pragma once

/*
This module functionality depends on the initialization of the file system
(SPIFFS or LittleFS) before calling the begin() method of the FileLogger class.

We recommend that the file system be initialized in the setup() of your main
program, before any logging operation.
*/

#include "SensorsManager.h"
#include "Utils.h"
#include "config.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <type_traits>
#include <vector>

namespace Storage {

class FileLogger {
public:
  FileLogger();
  void begin();
  ~FileLogger();

  //-------------- BEGIN LOG METHOD --------------
  template <typename T> bool logData(const T &data) {
    if constexpr (std::is_same_v<T, SensorsManager::SensorData>) {

      return writeToFile(getSensorsLogDirectory(), data.temperature,
                         data.humidity, data.soilMoisture, data.timestamp);
    }

    // Se no futuro você tiver dados de inferência:
    /*
    else if constexpr (std::is_same_v<T, InferenceManager::InferenceData>) {
    std::string fullPath = std::string(Config::DataLog::LittleFS_MOUNT_POINT) +
    getInferenceLogDirectory(); return writeToFile(fullPath.c_str(),
    data.resultado, data.confianca, data.timestamp);
    }
    */

    return false; // Retorna false se passar um tipo não suportado
  }
  //-------------- END LOG METHOD --------------

  std::vector<SensorsManager::SensorData> readAllSensorsLog();
  std::vector<SensorsManager::SensorData> readRecentSensorsLog(size_t n);

private:
  /*
  we need
  verify if log .csv exists
  select specific directory for logs
  save there logs plus timestamp
  be able to read log
  to write new log

  */
  template <typename... Args>
  bool writeToFile(const char *filename, Args... args);
  bool createFile(const char *filename, const char *header = nullptr);
  const char *getSensorsLogDirectory();
  const char *getInferenceLogDirectory();
  bool createReadingsLogFile();
  bool createInferenceLogFile();

  template <typename T>
  void writeArg(std::ofstream &file, const T &value) {
    file << value;
  }
};

}; // namespace Storage