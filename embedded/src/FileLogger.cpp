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
  SensorsManager::SensorData data;
  std::stringstream ss(line);
  std::string item;

  // Formato esperado na escrita: temperature, humidity, soilMoisture, timestamp

  // 1. Temperatura (float)
  if (std::getline(ss, item, ',')) {
    data.temperature = std::stoi(item);
  }
  // 2. Umidade (float)
  if (std::getline(ss, item, ',')) {
    data.humidity = std::stoi(item);
  }
  // 3. Umidade do Solo (int)
  if (std::getline(ss, item, ',')) {
    data.soilMoisture = std::stoi(item);
  }

  if (std::getline(ss, item, ',')) {
    data.timestamp = item.c_str();
  }

  return data;
}

std::vector<SensorsManager::SensorData> FileLogger::readAllSensorsLog() {
  std::vector<SensorsManager::SensorData> readings;
  std::ifstream file(getSensorsLogDirectory());

  if (!file.is_open())
    return readings; // vetor vazio

  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty()) {
      readings.push_back(parseCSVLine(line));
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

  if (limit > allReadings.size())
    limit = allReadings.size();

  // Copia apenas os últimos 'limit' elementos do vetor de trás para frente,
  // garantindo que o primeiro item do vetor de saída seja o mais recente de
  // todos.
  for (size_t i = 0; i < limit; ++i) {
    recentReadings.push_back(allReadings[allReadings.size() - 1 - i]);
  }

  return recentReadings;
}

} // namespace Storage