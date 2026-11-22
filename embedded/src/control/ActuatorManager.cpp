#include "ActuatorManager.hpp"
#include "TimeUtils.hpp"
#include "config.hpp"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "ActuatorManager";

esp_err_t ActuatorManager::init() {
  // Define o nivel de repouso ANTES de habilitar saida para evitar pulso no boot
  const int idleLevel = Config::Hardware::RELAY_ACTIVE_LOW ? 1 : 0;
  gpio_reset_pin(Config::Pins::RELAY_VALVE_PIN);
  gpio_set_level(Config::Pins::RELAY_VALVE_PIN, idleLevel);
  gpio_config_t relayCfg{};
  relayCfg.pin_bit_mask = (1ULL << Config::Pins::RELAY_VALVE_PIN);
  relayCfg.mode = GPIO_MODE_OUTPUT;
  relayCfg.pull_up_en = GPIO_PULLUP_DISABLE;
  relayCfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  relayCfg.intr_type = GPIO_INTR_DISABLE;
  esp_err_t err = gpio_config(&relayCfg);
  if (err != ESP_OK) return err;
  gpio_set_level(Config::Pins::RELAY_VALVE_PIN, idleLevel);

  gpio_reset_pin(Config::Pins::STATUS_LED_PIN);
  gpio_config_t ledCfg{};
  ledCfg.pin_bit_mask = (1ULL << Config::Pins::STATUS_LED_PIN);
  ledCfg.mode = GPIO_MODE_OUTPUT;
  ledCfg.pull_up_en = GPIO_PULLUP_DISABLE;
  ledCfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  ledCfg.intr_type = GPIO_INTR_DISABLE;
  err = gpio_config(&ledCfg);
  if (err != ESP_OK) return err;

  m_valveOpen = false;
  m_inCooldown = false;
  m_emergencyStopped = false;
  m_thermalStressAlert = false;
  m_consecutiveIrrigateCount = 0;
  m_initialized = true;
  setLedPhysical(true);

  ESP_LOGI(TAG, "Atuadores iniciados (RELAY GPIO %d, ACTIVE_LOW=%s, LED GPIO %d).",
           Config::Pins::RELAY_VALVE_PIN,
           Config::Hardware::RELAY_ACTIVE_LOW ? "true" : "false",
           Config::Pins::STATUS_LED_PIN);
  return ESP_OK;
}

void ActuatorManager::setValvePhysical(bool open) {
  if (!m_initialized) return;
  const int level = Config::Hardware::RELAY_ACTIVE_LOW ? (open ? 0 : 1) : (open ? 1 : 0);
  gpio_set_level(Config::Pins::RELAY_VALVE_PIN, level);
  m_valveOpen = open;
}

void ActuatorManager::setLedPhysical(bool on) {
  if (!m_initialized) return;
  gpio_set_level(Config::Pins::STATUS_LED_PIN, on ? 1 : 0);
  m_ledState = on;
}

void ActuatorManager::applyDecision(const InferenceResult &result, const SystemConfig &cfg) {
  if (!m_initialized) return;

  if (!result.valid) {
    emergencyStop();
    return;
  }

  m_emergencyStopped = false;
  m_thermalStressAlert = result.thermalStressAlert;
  const int64_t nowMs = TimeUtils::getMillis();

  // Fechamento imediato em OFF ou THERMAL_STRESS_WET_SOIL
  if (!result.shouldIrrigate || result.decision != IrrigationDecision::IRRIGATE) {
    m_consecutiveIrrigateCount = 0;
    if (m_valveOpen) {
      setValvePhysical(false);
      m_inCooldown = true;
      m_cooldownStartedAtMs = nowMs;
      ESP_LOGI(TAG, "Valvula FECHADA (%s). Cooldown iniciado (%lu ms).",
               result.thermalStressAlert ? "ESTRESSE TERMICO" : "OFF",
               static_cast<unsigned long>(cfg.cooldownMs));
    }
    return;
  }

  // Verificacao de cooldown
  if (m_inCooldown) {
    if ((nowMs - m_cooldownStartedAtMs) < static_cast<int64_t>(cfg.cooldownMs)) {
      ESP_LOGI(TAG, "Irrigacao solicitada, mas em cooldown (%lld ms restantes).",
               static_cast<long long>(cfg.cooldownMs - (nowMs - m_cooldownStartedAtMs)));
      return;
    }
    m_inCooldown = false;
  }

  if (m_valveOpen) {
    return; // Ja aberta; timeout eh monitorado em update()
  }

  // Debounce anti-chattering por N confirmacoes consecutivas
  if (m_consecutiveIrrigateCount < 255) {
    m_consecutiveIrrigateCount++;
  }
  if (m_consecutiveIrrigateCount >= Config::Hardware::CONSECUTIVE_IRRIGATE_CONFIRMATIONS) {
    setValvePhysical(true);
    m_valveOpenedAtMs = nowMs;
    m_consecutiveIrrigateCount = 0;
    ESP_LOGI(TAG, "Valvula ABERTA (timeout=%lu ms).",
             static_cast<unsigned long>(cfg.maxIrrigationMs));
  } else {
    ESP_LOGI(TAG, "Confirmacao de irrigacao %u/%u.",
             m_consecutiveIrrigateCount,
             Config::Hardware::CONSECUTIVE_IRRIGATE_CONFIRMATIONS);
  }
}

void ActuatorManager::update(const SystemConfig &cfg) {
  if (!m_initialized) return;
  const int64_t nowMs = TimeUtils::getMillis();

  if (m_valveOpen && (nowMs - m_valveOpenedAtMs) >= static_cast<int64_t>(cfg.maxIrrigationMs)) {
    setValvePhysical(false);
    m_inCooldown = true;
    m_cooldownStartedAtMs = nowMs;
    m_consecutiveIrrigateCount = 0;
    ESP_LOGW(TAG, "TIMEOUT hidraulico atingido (%lu ms). Valvula FECHADA e cooldown iniciado.",
             static_cast<unsigned long>(cfg.maxIrrigationMs));
  }

  if (m_inCooldown && (nowMs - m_cooldownStartedAtMs) >= static_cast<int64_t>(cfg.cooldownMs)) {
    m_inCooldown = false;
    ESP_LOGI(TAG, "Cooldown concluido.");
  }

  updateStatusLed(nowMs);
}

void ActuatorManager::emergencyStop() {
  if (m_valveOpen) {
    ESP_LOGE(TAG, "EMERGENCY STOP: fechando valvula imediatamente!");
  }
  setValvePhysical(false);
  m_consecutiveIrrigateCount = 0;
  m_emergencyStopped = true;
}

void ActuatorManager::updateStatusLed(int64_t nowMs) {
  if (m_emergencyStopped) {
    // Pisca rapido continuo (150 ms)
    if (nowMs - m_lastLedToggleMs >= 150) {
      m_lastLedToggleMs = nowMs;
      setLedPhysical(!m_ledState);
    }
    return;
  }

  if (m_thermalStressAlert) {
    // Pisca duplo rapido em janela de 2000 ms
    const int64_t phase = nowMs % 2000;
    const bool on = (phase < 120) || (phase >= 240 && phase < 360);
    if (on != m_ledState) setLedPhysical(on);
    return;
  }

  if (m_valveOpen) {
    // Pisca medio (500 ms) durante irrigacao
    if (nowMs - m_lastLedToggleMs >= 500) {
      m_lastLedToggleMs = nowMs;
      setLedPhysical(!m_ledState);
    }
    return;
  }

  // Operacao normal: pulso suave a cada 2s
  const int64_t phase = nowMs % 2000;
  const bool on = (phase < 1500);
  if (on != m_ledState) setLedPhysical(on);
}

ActuatorStatus ActuatorManager::getStatus() const {
  ActuatorStatus st{};
  const int64_t nowMs = TimeUtils::getMillis();
  st.valveOpen = m_valveOpen;
  st.inCooldown = m_inCooldown;
  st.emergencyStopped = m_emergencyStopped;
  st.thermalStressAlert = m_thermalStressAlert;
  st.consecutiveIrrigateCount = m_consecutiveIrrigateCount;
  if (m_valveOpen && nowMs >= m_valveOpenedAtMs) {
    st.openElapsedMs = static_cast<uint32_t>(nowMs - m_valveOpenedAtMs);
  }
  if (m_inCooldown && nowMs >= m_cooldownStartedAtMs) {
    const int64_t rem = static_cast<int64_t>(Config::Defaults::COOLDOWN_MS) - (nowMs - m_cooldownStartedAtMs);
    st.cooldownRemainingMs = rem > 0 ? static_cast<uint32_t>(rem) : 0;
  }
  return st;
}
