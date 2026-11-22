#include "ModelManager.hpp"
#include "TimeUtils.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "model_data.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include <algorithm>
#include <cmath>
#include <cstring>

static const char *TAG = "ModelManager";

ModelManager::~ModelManager() {
  if (m_interpreter != nullptr) {
    delete m_interpreter;
    m_interpreter = nullptr;
  }
}

esp_err_t ModelManager::init() {
  m_model = tflite::GetModel(g_model_data);
  if (m_model == nullptr) {
    ESP_LOGE(TAG, "Falha ao carregar flatbuffer g_model_data.");
    return ESP_FAIL;
  }

  if (m_model->version() != TFLITE_SCHEMA_VERSION) {
    ESP_LOGE(TAG, "Versao de schema TFLite incompativel: %lu (esperado %d).",
             static_cast<unsigned long>(m_model->version()), TFLITE_SCHEMA_VERSION);
    return ESP_ERR_INVALID_VERSION;
  }

  static tflite::MicroMutableOpResolver<3> resolver;
  static bool resolverReady = false;
  if (!resolverReady) {
    if (resolver.AddFullyConnected() != kTfLiteOk) return ESP_FAIL;
    if (resolver.AddSoftmax() != kTfLiteOk) return ESP_FAIL;
    if (resolver.AddRelu() != kTfLiteOk) return ESP_FAIL;
    resolverReady = true;
  }

  if (m_interpreter != nullptr) {
    delete m_interpreter;
    m_interpreter = nullptr;
  }

  m_interpreter = new tflite::MicroInterpreter(
      m_model, resolver, m_tensorArena, TENSOR_ARENA_SIZE);
  if (m_interpreter == nullptr) {
    ESP_LOGE(TAG, "Falha ao alocar MicroInterpreter.");
    return ESP_ERR_NO_MEM;
  }

  if (m_interpreter->AllocateTensors() != kTfLiteOk) {
    ESP_LOGE(TAG, "AllocateTensors() falhou (arena=%u bytes).",
             static_cast<unsigned>(TENSOR_ARENA_SIZE));
    return ESP_ERR_NO_MEM;
  }

  TfLiteTensor *input = m_interpreter->input(0);
  TfLiteTensor *output = m_interpreter->output(0);
  if (input == nullptr || output == nullptr ||
      input->type != kTfLiteInt8 || output->type != kTfLiteInt8 ||
      input->dims->size < 2 || input->dims->data[1] != TinyMLConfig::INPUT_FEATURE_COUNT ||
      output->dims->size < 2 || output->dims->data[1] != TinyMLConfig::OUTPUT_CLASS_COUNT) {
    ESP_LOGE(TAG, "Dimensoes ou tipos de tensores invalidos (esperado INT8 [1,27] -> [1,3]).");
    return ESP_ERR_INVALID_ARG;
  }

  m_initialized = true;
  ESP_LOGI(TAG, "TFLite Micro INT8 pronto! Arena usada: %zu / %u bytes.",
           m_interpreter->arena_used_bytes(), static_cast<unsigned>(TENSOR_ARENA_SIZE));
  return ESP_OK;
}

size_t ModelManager::getArenaUsedBytes() const {
  return m_interpreter ? m_interpreter->arena_used_bytes() : 0;
}

InferenceResult ModelManager::runRawInference(float tempC, float humPct, float soilPct,
                                              const AgronomicContext &ctx) const {
  InferenceResult res{};
  TimeUtils::getFormattedTimestamp(res.timestamp, sizeof(res.timestamp));

  if (!m_initialized || m_interpreter == nullptr) {
    return res;
  }

  float features[TinyMLConfig::INPUT_FEATURE_COUNT] = {};
  if (!TinyMLConfig::encodeFeatures27(tempC, humPct, soilPct,
                                      ctx.soilType, ctx.cropId, ctx.seedlingStage,
                                      features)) {
    ESP_LOGW(TAG, "Leitura invalida ou sentinela detectado (T=%.1f, H=%.1f, S=%.1f). Abortando.",
             tempC, humPct, soilPct);
    return res;
  }

  TfLiteTensor *input = m_interpreter->input(0);
  TfLiteTensor *output = m_interpreter->output(0);

  const float inScale = input->params.scale;
  const int32_t inZp = input->params.zero_point;
  for (uint8_t i = 0; i < TinyMLConfig::INPUT_FEATURE_COUNT; ++i) {
    const int32_t q = static_cast<int32_t>(std::lround(features[i] / inScale)) + inZp;
    input->data.int8[i] = static_cast<int8_t>(std::clamp<int32_t>(q, -128, 127));
  }

  const int64_t tStartUs = esp_timer_get_time();
  const TfLiteStatus invokeStatus = m_interpreter->Invoke();
  const int64_t tEndUs = esp_timer_get_time();
  res.latencyUs = static_cast<uint32_t>(tEndUs - tStartUs);

  if (invokeStatus != kTfLiteOk) {
    ESP_LOGE(TAG, "m_interpreter->Invoke() falhou.");
    return res;
  }

  const float outScale = output->params.scale;
  const int32_t outZp = output->params.zero_point;
  uint8_t bestCls = 0;
  float bestProb = -1.0f;

  for (uint8_t c = 0; c < TinyMLConfig::OUTPUT_CLASS_COUNT; ++c) {
    const float p = (static_cast<float>(output->data.int8[c]) - static_cast<float>(outZp)) * outScale;
    res.probabilities[c] = std::clamp(p, 0.0f, 1.0f);
    if (res.probabilities[c] > bestProb) {
      bestProb = res.probabilities[c];
      bestCls = c;
    }
  }

  res.decision = static_cast<IrrigationDecision>(bestCls);
  res.confidence = bestProb;

  // Fallback fisico complementar: solo ja umido (>=65%) sob calor extremo (>=32 C)
  if (tempC >= static_cast<float>(TinyMLConfig::THERMAL_STRESS_TEMP_MIN_C) &&
      soilPct >= static_cast<float>(TinyMLConfig::THERMAL_STRESS_SOIL_CAP_PCT)) {
    if (res.decision == IrrigationDecision::IRRIGATE ||
        (res.decision == IrrigationDecision::OFF &&
         res.probabilities[2] >= ctx.thermalStressThreshold)) {
      res.decision = IrrigationDecision::THERMAL_STRESS_WET_SOIL;
      res.fallbackTriggered = true;
    }
  }

  res.thermalStressAlert = (res.decision == IrrigationDecision::THERMAL_STRESS_WET_SOIL);
  res.shouldIrrigate = (res.decision == IrrigationDecision::IRRIGATE &&
                        res.confidence >= ctx.confidenceThreshold);
  res.valid = true;
  return res;
}

InferenceResult ModelManager::predict(const SensorData &data, const AgronomicContext &ctx) const {
  if (!data.valid) {
    InferenceResult invalid{};
    TimeUtils::getFormattedTimestamp(invalid.timestamp, sizeof(invalid.timestamp));
    return invalid;
  }
  return runRawInference(
      data.getTemperatureC(),
      static_cast<float>(data.humidity),
      static_cast<float>(data.soilMoisture),
      ctx);
}

bool ModelManager::runGoldenSelfTest() const {
  if (!m_initialized) return false;

  bool allOk = true;
  for (uint8_t i = 0; i < TinyMLConfig::GOLDEN_VECTOR_COUNT; ++i) {
    const auto &gv = TinyMLConfig::GOLDEN_VECTORS[i];
    AgronomicContext ctx{};
    ctx.soilType = gv.soilId;
    ctx.cropId = gv.cropId;
    ctx.seedlingStage = gv.stageId;

    const InferenceResult res = runRawInference(gv.tempC, gv.humPct, gv.soilPct, ctx);
    float maxProbDiff = 0.0f;
    for (int c = 0; c < 3; ++c) {
      maxProbDiff = std::max(maxProbDiff, std::fabs(res.probabilities[c] - gv.expectedProbs[c]));
    }

    const bool classMatch = (static_cast<uint8_t>(res.decision) == gv.expectedFinalClass);
    const bool probMatch = (maxProbDiff <= 0.02f);
    if (!res.valid || !classMatch || !probMatch) {
      allOk = false;
      ESP_LOGE(TAG, "Golden[%u] FALHOU: pred=%u (esp=%u), maxDiff=%.4f",
               i, static_cast<unsigned>(res.decision), gv.expectedFinalClass, maxProbDiff);
    } else {
      ESP_LOGI(TAG, "Golden[%u] OK: class=%u, P=[%.3f, %.3f, %.3f], lat=%lu us",
               i, static_cast<unsigned>(res.decision),
               res.probabilities[0], res.probabilities[1], res.probabilities[2],
               static_cast<unsigned long>(res.latencyUs));
    }
  }
  return allOk;
}
