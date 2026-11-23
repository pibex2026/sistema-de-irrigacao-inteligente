#include "unity.h"
#include "model_data.h"
#include <cmath>
#include <limits>

void setUp(void) {}
void tearDown(void) {}

static void test_sensor_sentinel_and_range_validation(void) {
  TEST_ASSERT_TRUE(TinyMLConfig::isValidSensorReading(25.4f, 60.0f, 45.0f));

  // Sentinelas de erro de hardware devem ser rejeitados
  TEST_ASSERT_FALSE(TinyMLConfig::isValidSensorReading(-128.0f, 60.0f, 45.0f));
  TEST_ASSERT_FALSE(TinyMLConfig::isValidSensorReading(-999.0f, 60.0f, 45.0f));
  TEST_ASSERT_FALSE(TinyMLConfig::isValidSensorReading(25.0f, 255.0f, 45.0f));
  TEST_ASSERT_FALSE(TinyMLConfig::isValidSensorReading(25.0f, 60.0f, 255.0f));

  // NaN e limites fisicos invalidos
  TEST_ASSERT_FALSE(TinyMLConfig::isValidSensorReading(std::numeric_limits<float>::quiet_NaN(), 50.0f, 50.0f));
  TEST_ASSERT_FALSE(TinyMLConfig::isValidSensorReading(-15.0f, 50.0f, 50.0f));
  TEST_ASSERT_FALSE(TinyMLConfig::isValidSensorReading(70.0f, 50.0f, 50.0f));
  TEST_ASSERT_FALSE(TinyMLConfig::isValidSensorReading(25.0f, 105.0f, 50.0f));
  TEST_ASSERT_FALSE(TinyMLConfig::isValidSensorReading(25.0f, 50.0f, -1.0f));
}

static void test_vpd_calculation_matches_tetens_reference(void) {
  const float vpdCoolHumid = TinyMLConfig::computeVpdKpa(22.0f, 78.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.58f, vpdCoolHumid);

  const float vpdHotDry = TinyMLConfig::computeVpdKpa(34.0f, 45.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 2.93f, vpdHotDry);

  const float vpdExtreme = TinyMLConfig::computeVpdKpa(38.0f, 40.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 3.97f, vpdExtreme);
}

static void test_encode_features27_for_all_golden_vectors(void) {
  float features[TinyMLConfig::INPUT_FEATURE_COUNT] = {};

  for (uint8_t idx = 0; idx < TinyMLConfig::GOLDEN_VECTOR_COUNT; ++idx) {
    const auto &gv = TinyMLConfig::GOLDEN_VECTORS[idx];
    const bool ok = TinyMLConfig::encodeFeatures27(
        gv.tempC, gv.humPct, gv.soilPct,
        gv.soilId, gv.cropId, gv.stageId,
        features);
    TEST_ASSERT_TRUE(ok);

    for (uint8_t i = 0; i < TinyMLConfig::INPUT_FEATURE_COUNT; ++i) {
      TEST_ASSERT_FALSE(std::isnan(features[i]));
      TEST_ASSERT_TRUE(features[i] >= 0.0f && features[i] <= 1.0f);
    }

    // Verifica one-hot encoding de contexto agronomico (ID 0 = all zeros)
    if (gv.soilId >= 1 && gv.soilId <= 7) {
      TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, features[3 + (gv.soilId - 1)]);
    }
    if (gv.cropId >= 1 && gv.cropId <= 5) {
      TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, features[10 + (gv.cropId - 1)]);
    }
    if (gv.stageId >= 1 && gv.stageId <= 8) {
      TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, features[15 + (gv.stageId - 1)]);
    }

    // Quantizacao INT8 deve permanecer na faixa [-128, 127]
    for (uint8_t i = 0; i < TinyMLConfig::INPUT_FEATURE_COUNT; ++i) {
      const int8_t q = TinyMLConfig::quantizeInput(features[i]);
      TEST_ASSERT_TRUE(q >= -128 && q <= 127);
    }
  }
}

static void test_encode_features27_rejects_invalid_readings(void) {
  float features[TinyMLConfig::INPUT_FEATURE_COUNT] = {};
  TEST_ASSERT_FALSE(TinyMLConfig::encodeFeatures27(-999.0f, 50.0f, 50.0f, 0, 0, 0, features));
  TEST_ASSERT_FALSE(TinyMLConfig::encodeFeatures27(25.0f, 255.0f, 50.0f, 0, 0, 0, features));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_sensor_sentinel_and_range_validation);
  RUN_TEST(test_vpd_calculation_matches_tetens_reference);
  RUN_TEST(test_encode_features27_for_all_golden_vectors);
  RUN_TEST(test_encode_features27_rejects_invalid_readings);
  return UNITY_END();
}
