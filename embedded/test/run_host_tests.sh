#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EMB_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
UNITY_DIR="${UNITY_DIR:-$HOME/.platformio/packages/framework-espidf/components/unity/unity/src}"

if [ ! -f "${UNITY_DIR}/unity.c" ]; then
  echo "Unity nao encontrado em: ${UNITY_DIR}" >&2
  exit 1
fi

BUILD_DIR="/tmp/irrigacao_fw_tests"
mkdir -p "${BUILD_DIR}"

echo "[1/3] Compilando e executando test_tinyml_golden..."
g++ -std=gnu++17 -Wall -Wextra \
  -I"${UNITY_DIR}" -I"${EMB_DIR}/include" \
  "${UNITY_DIR}/unity.c" "${SCRIPT_DIR}/test_tinyml_golden.cpp" \
  -o "${BUILD_DIR}/test_tinyml_golden"
"${BUILD_DIR}/test_tinyml_golden"

echo "[2/3] Compilando e executando test_config_sanitize..."
g++ -std=gnu++17 -Wall -Wextra \
  -I"${UNITY_DIR}" -I"${SCRIPT_DIR}/stubs" -I"${EMB_DIR}/include" \
  "${UNITY_DIR}/unity.c" "${SCRIPT_DIR}/test_config_sanitize.cpp" \
  -o "${BUILD_DIR}/test_config_sanitize"
"${BUILD_DIR}/test_config_sanitize"

echo "[3/3] Compilando e executando test_actuator_logic..."
g++ -std=gnu++17 -Wall -Wextra \
  -I"${UNITY_DIR}" -I"${SCRIPT_DIR}/stubs" -I"${EMB_DIR}/include" -I"${EMB_DIR}/src/control" \
  "${UNITY_DIR}/unity.c" "${EMB_DIR}/src/control/ActuatorManager.cpp" "${SCRIPT_DIR}/test_actuator_logic.cpp" \
  -o "${BUILD_DIR}/test_actuator_logic"
"${BUILD_DIR}/test_actuator_logic"

echo "ALL_FIRMWARE_HOST_TESTS_PASSED"
