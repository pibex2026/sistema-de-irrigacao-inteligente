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
#include <Arduino.h>
#include <cstdint>
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

    return false; // Retorna false se passar um tipo não suportado
  }
  //-------------- END LOG METHOD --------------

  std::vector<SensorsManager::SensorData> readAllSensorsLog();
  std::vector<SensorsManager::SensorData> readRecentSensorsLog(size_t n);

private:
  template <typename T>
  void writeArg(std::ofstream &file, const T &value) {
    if constexpr (std::is_same_v<T, int8_t> || std::is_same_v<T, uint8_t>) {
      file << static_cast<int>(value);
    } else {
      file << value;
    }
  }

  template <typename... Args>
  bool writeToFile(const char *filename, Args... args) {
    std::ofstream file(filename, std::ios::app);
    if (!file.is_open()) {
      return false;
    }
    size_t index = 0;
    auto writeWithComma = [&](const auto &val) {
      if (index++ > 0) {
        file << ",";
      }
      writeArg(file, val);
    };
    (writeWithComma(args), ...);
    file << "\n";
    file.close();
    return true;
  }

  bool createFile(const char *filename, const char *header = nullptr);
  const char *getSensorsLogDirectory();
  const char *getInferenceLogDirectory();
  bool createReadingsLogFile();
  bool createInferenceLogFile();
};

}; // namespace Storage