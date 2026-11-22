#include "FileLogger.h"

namespace Storage {

FileLogger::FileLogger() {}

FileLogger::~FileLogger() {}

void FileLogger::begin() {

  bool readingsOk = createReadingsLogFile();
  bool inferenceOk = createInferenceLogFile();

  if (readingsOk && inferenceOk) {
    std::printf("[Storage] FileLogger iniciado e arquivos garantidos.\n");
  } else {
    std::printf(
        "[Storage] ALERTA: Falha ao garantir arquivos de log no disco.\n");
  }
}

const char *FileLogger::getSensorsLogDirectory() {

  static const String fullPathSensors =
      String(Config::DataLog::LittleFS_MOUNT_POINT) +
      String(Config::DataLog::SENSORS_READINGS_LOG);
  return fullPathSensors.c_str();
}

const char *FileLogger::getInferenceLogDirectory() {
  static const String fullPathInference =
      String(Config::DataLog::LittleFS_MOUNT_POINT) +
      String(Config::DataLog::INFERENCE_LOG);
  return fullPathInference.c_str();
}

bool FileLogger::createFile(const char *filename, const char *header) {

  std::ifstream checkFile(filename);
  bool exists = checkFile.good();
  checkFile.close();
  if (exists)
    return true;

  std::ofstream file(filename);
  if (!file)
    return false;

  if (header != nullptr)
    file << header << "\n";

  file.close();
  return true;
}

bool FileLogger::createReadingsLogFile() {
  const char *sensorsHeader = "temperatura,umidade,umidade_solo,timestamp";
  return FileLogger::createFile(getSensorsLogDirectory(), sensorsHeader);
}

bool FileLogger::createInferenceLogFile() {
  const char *inferenceHeader = "inference_result, timestamp";
  return FileLogger::createFile(getInferenceLogDirectory(), inferenceHeader);
}

SensorsManager::SensorData parseCSVLine(const std::string &line) {
  SensorsManager::SensorData data{};
  std::stringstream ss(line);
  std::string item;

  try {
    if (std::getline(ss, item, ',')) {
      data.temperature = static_cast<int8_t>(std::stoi(item));
    }

    if (std::getline(ss, item, ',')) {
      data.humidity = static_cast<uint8_t>(std::stoi(item));
    }

    if (std::getline(ss, item, ',')) {
      data.soilMoisture = static_cast<uint8_t>(std::stoi(item));
    }

    if (std::getline(ss, item, ',')) {
      strncpy(data.timestamp, item.c_str(), sizeof(data.timestamp) - 1);
      data.timestamp[sizeof(data.timestamp) - 1] = '\0';
    }
  } catch (...) {
    return SensorsManager::SensorData{};
  }

  return data;
}

std::vector<SensorsManager::SensorData> FileLogger::readAllSensorsLog() {
  std::vector<SensorsManager::SensorData> readings;
  std::ifstream file(getSensorsLogDirectory());

  if (!file.is_open())
    return readings; 

  std::string line;
  bool isFirstLine = true;
  while (std::getline(file, line)) {
    if (line.empty()) continue;

    // Skip header line
    if (isFirstLine) {
      isFirstLine = false;
      if (line.rfind("temperatura", 0) == 0) {
        continue;
      }
    }

    SensorsManager::SensorData data = parseCSVLine(line);
    if (data.timestamp[0] != '\0') {
      readings.push_back(data);
    }
  }

  file.close();
  return readings;
}

std::vector<SensorsManager::SensorData>
FileLogger::readRecentSensorsLog(size_t limit) {
  std::vector<SensorsManager::SensorData> allReadings = readAllSensorsLog();
  std::vector<SensorsManager::SensorData> recentReadings;

  if (allReadings.empty() || limit == 0)
    return recentReadings;

  size_t count = (limit > allReadings.size()) ? allReadings.size() : limit;
  recentReadings.reserve(count);

  // Copia apenas os últimos 'limit' elementos do vetor de trás para frente,
  // garantindo que o primeiro item do vetor de saída seja o mais recente de
  // todos.
  for (size_t i = 0; i < count; ++i) {
    recentReadings.push_back(allReadings[allReadings.size() - 1 - i]);
  }

  return recentReadings;
}

} // namespace Storage