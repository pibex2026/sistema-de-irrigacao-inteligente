#include "unity.h"
#include "ConfigSanitizer.hpp"
#include <cstring>

void setUp(void) {}
void tearDown(void) {}

static void test_sanitize_clamps_out_of_range_values(void) {
  SystemConfig cfg{};
  cfg.soilType = 99;
  cfg.cropId = 42;
  cfg.seedlingStage = 15;
  cfg.confidenceThreshold = 0.10f;
  cfg.thermalStressThreshold = 0.99f;
  cfg.maxIrrigationMs = 100;
  cfg.cooldownMs = 99999999;
  cfg.sensorReadIntervalMs = 100;
  cfg.logIntervalMs = 500;
  cfg.telemetryIntervalMs = 1000;

  sanitizeSystemConfig(cfg, 2);

  TEST_ASSERT_EQUAL_UINT32(2, cfg.schemaVersion);
  TEST_ASSERT_EQUAL_UINT8(0, cfg.soilType);
  TEST_ASSERT_EQUAL_UINT8(0, cfg.cropId);
  TEST_ASSERT_EQUAL_UINT8(0, cfg.seedlingStage);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.30f, cfg.confidenceThreshold);
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.85f, cfg.thermalStressThreshold);
  TEST_ASSERT_EQUAL_UINT32(5000, cfg.maxIrrigationMs);
  TEST_ASSERT_EQUAL_UINT32(3600000, cfg.cooldownMs);
  TEST_ASSERT_EQUAL_UINT32(2500, cfg.sensorReadIntervalMs);
  TEST_ASSERT_EQUAL_UINT32(10000, cfg.logIntervalMs);
  TEST_ASSERT_EQUAL_UINT32(5000, cfg.telemetryIntervalMs);
}

static void test_sanitize_resets_invalid_or_equal_adc_calibration(void) {
  SystemConfig cfg{};
  cfg.soilAdcDry = 2000;
  cfg.soilAdcWet = 2000;
  sanitizeSystemConfig(cfg);
  TEST_ASSERT_EQUAL_UINT16(Config::Defaults::SOIL_ADC_DRY, cfg.soilAdcDry);
  TEST_ASSERT_EQUAL_UINT16(Config::Defaults::SOIL_ADC_WET, cfg.soilAdcWet);

  cfg.soilAdcDry = 5000;
  cfg.soilAdcWet = 4500;
  sanitizeSystemConfig(cfg);
  TEST_ASSERT_EQUAL_UINT16(Config::Defaults::SOIL_ADC_DRY, cfg.soilAdcDry);
  TEST_ASSERT_EQUAL_UINT16(Config::Defaults::SOIL_ADC_WET, cfg.soilAdcWet);
}

static void test_sanitize_preserves_valid_config_and_null_terminates_strings(void) {
  SystemConfig cfg{};
  cfg.soilType = 2;
  cfg.cropId = 1;
  cfg.seedlingStage = 4;
  cfg.confidenceThreshold = 0.70f;
  cfg.thermalStressThreshold = 0.45f;
  cfg.soilAdcDry = 3400;
  cfg.soilAdcWet = 1100;
  std::memset(cfg.backendUrl, 'a', sizeof(cfg.backendUrl));
  std::memset(cfg.deviceApiKey, 'k', sizeof(cfg.deviceApiKey));

  sanitizeSystemConfig(cfg);

  TEST_ASSERT_EQUAL_UINT8(2, cfg.soilType);
  TEST_ASSERT_EQUAL_UINT8(1, cfg.cropId);
  TEST_ASSERT_EQUAL_UINT8(4, cfg.seedlingStage);
  TEST_ASSERT_EQUAL_UINT16(3400, cfg.soilAdcDry);
  TEST_ASSERT_EQUAL_UINT16(1100, cfg.soilAdcWet);
  TEST_ASSERT_EQUAL_CHAR('\0', cfg.backendUrl[sizeof(cfg.backendUrl) - 1]);
  TEST_ASSERT_EQUAL_CHAR('\0', cfg.deviceApiKey[sizeof(cfg.deviceApiKey) - 1]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_sanitize_clamps_out_of_range_values);
  RUN_TEST(test_sanitize_resets_invalid_or_equal_adc_calibration);
  RUN_TEST(test_sanitize_preserves_valid_config_and_null_terminates_strings);
  return UNITY_END();
}
