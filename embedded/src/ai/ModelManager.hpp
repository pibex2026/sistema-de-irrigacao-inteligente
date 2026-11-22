#pragma once

#include "esp_err.h"
#include "types.hpp"

namespace tflite {
class MicroInterpreter;
struct Model;
} // namespace tflite

class ModelManager {
public:
  ModelManager() = default;
  ~ModelManager();

  esp_err_t init();
  InferenceResult predict(const SensorData &data, const AgronomicContext &ctx = AgronomicContext{}) const;
  bool runGoldenSelfTest() const;
  size_t getArenaUsedBytes() const;

private:
  static constexpr size_t TENSOR_ARENA_SIZE = 12 * 1024;

  const tflite::Model *m_model = nullptr;
  tflite::MicroInterpreter *m_interpreter = nullptr;
  alignas(16) uint8_t m_tensorArena[TENSOR_ARENA_SIZE] = {};
  bool m_initialized = false;

  InferenceResult runRawInference(float tempC, float humPct, float soilPct,
                                  const AgronomicContext &ctx) const;
};
