#include "unity.h"
#include "ActuatorManager.hpp"
#include "TimeUtils.hpp"
#include "config.hpp"
#include "driver/gpio.h"

void setUp(void) {
  g_mockMillis = 1000;
  for (int i = 0; i < 40; ++i) g_mockGpioLevel[i] = -1;
}

void tearDown(void) {}

static InferenceResult makeIrrigateDecision(bool irrigate, bool valid = true) {
  InferenceResult res{};
  res.valid = valid;
  res.decision = irrigate ? IrrigationDecision::IRRIGATE : IrrigationDecision::OFF;
  res.shouldIrrigate = irrigate;
  res.thermalStressAlert = false;
  return res;
}

static void test_init_sets_relay_idle_and_valve_closed(void) {
  ActuatorManager act;
  TEST_ASSERT_EQUAL(ESP_OK, act.init());

  const ActuatorStatus st = act.getStatus();
  TEST_ASSERT_FALSE(st.valveOpen);
  TEST_ASSERT_FALSE(st.inCooldown);
  TEST_ASSERT_FALSE(st.emergencyStopped);
  // RELAY_ACTIVE_LOW = true -> idle = 1
  TEST_ASSERT_EQUAL(1, g_mockGpioLevel[Config::Pins::RELAY_VALVE_PIN]);
}

static void test_debounce_requires_consecutive_confirmations_and_enforces_timeout_and_custom_cooldown(void) {
  ActuatorManager act;
  act.init();

  SystemConfig cfg{};
  cfg.maxIrrigationMs = 30000;
  cfg.cooldownMs = 120000; // customizado diferente do default (300000)

  // 1a confirmacao nao abre a valvula ainda
  act.applyDecision(makeIrrigateDecision(true), cfg);
  TEST_ASSERT_FALSE(act.getStatus().valveOpen);
  TEST_ASSERT_EQUAL_UINT8(1, act.getStatus().consecutiveIrrigateCount);

  // 2a confirmacao consecutiva abre a valvula
  act.applyDecision(makeIrrigateDecision(true), cfg);
  TEST_ASSERT_TRUE(act.getStatus().valveOpen);
  TEST_ASSERT_EQUAL(0, g_mockGpioLevel[Config::Pins::RELAY_VALVE_PIN]); // active low -> 0

  // Avanca 29s: continua aberta
  g_mockMillis += 29000;
  act.update(cfg);
  TEST_ASSERT_TRUE(act.getStatus().valveOpen);

  // Avanca mais 1s (total 30s = maxIrrigationMs): fecha por timeout e inicia cooldown de 120s
  g_mockMillis += 1000;
  act.update(cfg);
  ActuatorStatus st = act.getStatus();
  TEST_ASSERT_FALSE(st.valveOpen);
  TEST_ASSERT_TRUE(st.inCooldown);
  TEST_ASSERT_EQUAL_UINT32(120000, st.cooldownRemainingMs);
  TEST_ASSERT_EQUAL(1, g_mockGpioLevel[Config::Pins::RELAY_VALVE_PIN]);

  // Avanca 60s: cooldownRemainingMs deve ser 60000 (nao baseado no default 300000)
  g_mockMillis += 60000;
  act.update(cfg);
  st = act.getStatus();
  TEST_ASSERT_TRUE(st.inCooldown);
  TEST_ASSERT_EQUAL_UINT32(60000, st.cooldownRemainingMs);

  // Avanca mais 60s: cooldown termina
  g_mockMillis += 60000;
  act.update(cfg);
  st = act.getStatus();
  TEST_ASSERT_FALSE(st.inCooldown);
  TEST_ASSERT_EQUAL_UINT32(0, st.cooldownRemainingMs);
}

static void test_manual_override_respects_timeout_and_emergency_stop(void) {
  ActuatorManager act;
  act.init();

  SystemConfig cfg{};
  cfg.maxIrrigationMs = 15000;
  cfg.cooldownMs = 45000;

  TEST_ASSERT_TRUE(act.manualOverride(true, cfg));
  TEST_ASSERT_TRUE(act.getStatus().valveOpen);

  // Timeout hidraulico tambem fecha o override manual
  g_mockMillis += 15000;
  act.update(cfg);
  TEST_ASSERT_FALSE(act.getStatus().valveOpen);
  TEST_ASSERT_TRUE(act.getStatus().inCooldown);

  // Emergency stop bloqueia novo override manual
  act.emergencyStop();
  TEST_ASSERT_TRUE(act.getStatus().emergencyStopped);
  TEST_ASSERT_FALSE(act.manualOverride(true, cfg));
  TEST_ASSERT_FALSE(act.getStatus().valveOpen);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_init_sets_relay_idle_and_valve_closed);
  RUN_TEST(test_debounce_requires_consecutive_confirmations_and_enforces_timeout_and_custom_cooldown);
  RUN_TEST(test_manual_override_respects_timeout_and_emergency_stop);
  return UNITY_END();
}
