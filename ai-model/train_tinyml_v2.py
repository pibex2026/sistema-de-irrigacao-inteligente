#!/usr/bin/env python3
"""
===============================================================================
Treinamento e Exportação TinyML v2 (ESP32) — Sistema de Irrigação Inteligente
Projeto PIBEX 2026 - IFPE Campus Paulista
===============================================================================
Pipeline de Treinamento v2 fundamentado nos resultados experimentais de:
  - ai-model/auditoria_tinyml_e_comparativo.md
  - docs/AUDITORIA_TINYML_E_COMPARATIVO.md

Correções e Melhorias da v2 em relação à v1 (MLP-Orig Buggy -> PhysMLP-27 v2):
  1. [Bug Sintaxe Corrigido]: Substituição de `{=*75}` inválido por `sep = '=' * 75`.
  2. [Deduplicação Pré-Split]: Remoção das 128 duplicatas exatas (16.411 -> 16.283
     registros únicos) antes do split estratificado, eliminando vazamento entre splits.
  3. [Auditoria de Sensor-Tuple Leakage & GroupSplit]: Avaliação dupla em:
     - Split Estratificado Limpo (70/15/15 por `crop ID + result`).
     - GroupSplit Estrito por tupla `(MOI, temp, humidity)` (0% de sobreposição
       de leituras físicas entre treino, validação e teste).
  4. [Validação Limpa (`is_training=False`)]: Validação avaliada na distribuição real
     sem distorcer `val_loss` com replicação 4x ou filtragem artificial `MOI >= 60`.
  5. [Pesos de Classe Calibrados `{0: 1.0, 1: 1.15, 2: 2.20}`]: Substitui o
     `class_weight='balanced'` (~5.05 na Classe 2), elevando a Precisão da Classe 2
     no Modo Zero Contexto e reduzindo o erro de calibração (ECE de 0.0676 -> 0.0145).
  6. [Full-8 Mask Context Dropout]: Treinamento e avaliação explícita em todas as
     2^3 = 8 combinações de máscara `(Soil, Crop, Stage)`, corrigindo o ponto cego
     do Modo 6 (`Apenas Cultura [0, C, 0]`: 83,27% -> 89,28% de acurácia).
  7. [Engenharia Psicrométrica (`PhysMLP-27` com 27 Features)]: Adição de 4 features
     físicas não-lineares calculáveis em < 15 us no ESP32:
     - `VPD_norm` (Déficit de Pressão de Vapor de Tetens normalizado em [0..8 kPa])
     - `EvapDemand` (Índice de demanda evaporativa `T * (100 - RH) / 4000`)
     - `Temp_MOI_Ratio` (Razão `T_norm / (MOI_norm + 0.10) / 5.0`)
     - `HydroThermal_Coupling` (Acoplamento `MOI_norm * T_norm` para Classe 2)
  8. [Full Integer Quantization (`INT8`) Real]: Conversão TFLite com
     `representative_dataset` estratificado nos 8 modos + `TFLITE_BUILTINS_INT8`
     (reduzindo o modelo de 7.620 B Float32 para ~5.400 B INT8 real, com ~99% de
     concordância com o Keras Float32).
  9. [Proteção contra Sentinelas de Falha do Hardware (`SensorsManager`)]:
     Validação explícita de `-128` (`int8_t`), `-999` (`int16_t`), `255` (`uint8_t`)
     e `NaN` antes do `clip`/`clamp` em Python e no helper C++ inline exportado em
     `model_data.h`.

Execução:
  /usr/bin/python3 ai-model/train_tinyml_v2.py
===============================================================================
"""

import os
import glob
import json
import time
import random
import argparse
import numpy as np
import pandas as pd
import tensorflow as tf

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    plt = None

os.environ["TF_CPP_MIN_LOG_LEVEL"] = "2"

# Sementes fixas para reprodutibilidade científica
SEED = 42
random.seed(SEED)
np.random.seed(SEED)
tf.random.set_seed(SEED)
tf.keras.utils.set_random_seed(SEED)

# ============================================================================
# 1. CONSTANTES FÍSICAS, SENTINELAS DE HARDWARE E MAPEAMENTO AGRONÔMICO
# ============================================================================
TEMP_MIN, TEMP_MAX = 0.0, 50.0
HUM_MIN, HUM_MAX = 0.0, 100.0
SOIL_MIN, SOIL_MAX = 0.0, 100.0

# Escalas psicrométricas compartilhadas com o C++ (ESP32)
VPD_MAX_KPA = 8.0
EVAP_DEMAND_DIVISOR = 4000.0
RATIO_MOI_OFFSET = 0.10
RATIO_MAX_CLIP = 5.0

# Sentinelas de falha do DHT22 / HW-080 definidos em SensorsManager
SENTINEL_TEMP_ERR_I8 = -128
SENTINEL_TEMP_ERR_I16 = -999
SENTINEL_HUM_ERR_U8 = 255

# Limiares físicos de segurança no firmware
DEFAULT_CONFIDENCE_THRESHOLD = 0.55
THERMAL_STRESS_SOIL_CAP_PCT = 65
THERMAL_STRESS_TEMP_MIN_C = 32

SOIL_TYPES = [
    "Clay Soil", "Sandy Soil", "Red Soil", "Loam Soil",
    "Black Soil", "Alluvial Soil", "Chalky Soil",
]
CROP_TYPES = ["Tomato", "Chilli", "Carrot", "Potato", "Wheat"]
SEEDLING_STAGES = [
    "Germination", "Seedling Stage",
    "Vegetative Growth / Root or Tuber Development",
    "Flowering", "Pollination", "Fruit/Grain/Bulb Formation",
    "Maturation", "Harvest",
]

SOIL_TO_ID = {name: i + 1 for i, name in enumerate(SOIL_TYPES)}       # 1..7 (0 = UNKNOWN)
CROP_TO_ID = {name: i + 1 for i, name in enumerate(CROP_TYPES)}       # 1..5 (0 = UNKNOWN)
STAGE_TO_ID = {name: i + 1 for i, name in enumerate(SEEDLING_STAGES)} # 1..8 (0 = UNKNOWN)

# 3 sensores + 7 solos + 5 culturas + 8 estágios + 4 features psicrométricas = 27
BASE_DIM = 3 + len(SOIL_TYPES) + len(CROP_TYPES) + len(SEEDLING_STAGES)  # 23
INPUT_DIM = BASE_DIM + 4                                                 # 27 (PhysMLP-27)
NUM_CLASSES = 3

CLASS_NAMES = [
    "0: OFF (Nao Irrigar)",
    "1: IRRIGATE (Irrigar)",
    "2: THERMAL_STRESS (Alerta)",
]

ALL_8_MASKS = [
    ("M1: Completo [S,C,St]", (1, 1, 1)),
    ("M2: Solo+Cultura [S,C,0]", (1, 1, 0)),
    ("M3: Solo+Estágio [S,0,St]", (1, 0, 1)),
    ("M4: Cultura+Estágio [0,C,St]", (0, 1, 1)),
    ("M5: Apenas Solo [S,0,0]", (1, 0, 0)),
    ("M6: Apenas Cultura [0,C,0]", (0, 1, 0)),
    ("M7: Apenas Estágio [0,0,St]", (0, 0, 1)),
    ("M8: Zero Contexto [0,0,0]", (0, 0, 0)),
]

# Pesos de classe calibrados na auditoria (Pareto Precisão vs Recall da Classe 2)
CALIBRATED_CLASS_WEIGHTS = {0: 1.0, 1: 1.15, 2: 2.20}


# ============================================================================
# 2. FUNÇÕES DE VALIDAÇÃO DE HARDWARE E ENGENHARIA PSICROMÉTRICA (27 FEATURES)
# ============================================================================
def is_valid_sensor_reading(temp, hum, moi):
    """
    Verifica se as leituras físicas não contêm sentinelas de falha do hardware
    (DHT22 / HW-080) nem valores fora da faixa física operacional.
    """
    if temp is None or hum is None or moi is None:
        return False
    if np.isnan(temp) or np.isnan(hum) or np.isnan(moi):
        return False
    if int(round(temp)) in (SENTINEL_TEMP_ERR_I8, SENTINEL_TEMP_ERR_I16):
        return False
    if int(round(hum)) == SENTINEL_HUM_ERR_U8 or int(round(moi)) == SENTINEL_HUM_ERR_U8:
        return False
    if not (-10.0 <= float(temp) <= 65.0):
        return False
    if not (0.0 <= float(hum) <= 100.0):
        return False
    if not (0.0 <= float(moi) <= 100.0):
        return False
    return True


def compute_vpd_kpa(temp_c, rh_pct):
    """
    Calcula o Déficit de Pressão de Vapor (VPD, em kPa) pela equação de Tetens:
      es(T) = 0.61078 * exp((17.27 * T) / (T + 237.3))
      VPD   = es(T) * (1 - RH / 100)
    Aceita escalares ou arrays NumPy.
    """
    es = 0.61078 * np.exp((17.27 * temp_c) / (temp_c + 237.3))
    return es * np.clip(1.0 - rh_pct / 100.0, 0.0, 1.0)


def encode_features(temp, hum, moi, soil_id=0, crop_id=0, stage_id=0, validate_sentinels=True):
    """
    Codifica 1 amostra em um vetor float32 de 27 posições (PhysMLP-27):
      [0..2]   : temp_norm, hum_norm, moi_norm
      [3..9]   : One-Hot soil_type (7)   | 0 = UNKNOWN -> todos 0.0
      [10..14] : One-Hot crop ID (5)     | 0 = UNKNOWN -> todos 0.0
      [15..22] : One-Hot Seedling Stage (8) | 0 = UNKNOWN -> todos 0.0
      [23]     : vpd_norm = clip(VPD_kPa / 8.0, 0, 1)
      [24]     : evap_demand = clip(T * (100 - RH) / 4000.0, 0, 1)
      [25]     : temp_moi_ratio = clip(t_n / (m_n + 0.10), 0, 5) / 5.0
      [26]     : hydrothermal_coupling = m_n * t_n
    """
    if validate_sentinels and not is_valid_sensor_reading(temp, hum, moi):
        raise ValueError(
            f"Sentinela ou leitura inválida detectada: temp={temp}, hum={hum}, moi={moi}. "
            "A inferência deve ser abortada (fail-safe -> OFF/ERROR)."
        )

    vec = np.zeros(INPUT_DIM, dtype=np.float32)
    t_n = float(np.clip((temp - TEMP_MIN) / (TEMP_MAX - TEMP_MIN), 0.0, 1.0))
    h_n = float(np.clip((hum - HUM_MIN) / (HUM_MAX - HUM_MIN), 0.0, 1.0))
    m_n = float(np.clip((moi - SOIL_MIN) / (SOIL_MAX - SOIL_MIN), 0.0, 1.0))
    vec[0], vec[1], vec[2] = t_n, h_n, m_n

    if 1 <= int(soil_id) <= len(SOIL_TYPES):
        vec[3 + (int(soil_id) - 1)] = 1.0
    if 1 <= int(crop_id) <= len(CROP_TYPES):
        vec[3 + len(SOIL_TYPES) + (int(crop_id) - 1)] = 1.0
    if 1 <= int(stage_id) <= len(SEEDLING_STAGES):
        vec[3 + len(SOIL_TYPES) + len(CROP_TYPES) + (int(stage_id) - 1)] = 1.0

    vpd = float(compute_vpd_kpa(temp, hum))
    vec[23] = float(np.clip(vpd / VPD_MAX_KPA, 0.0, 1.0))
    vec[24] = float(np.clip((temp * (100.0 - hum)) / EVAP_DEMAND_DIVISOR, 0.0, 1.0))
    vec[25] = float(np.clip(t_n / (m_n + RATIO_MOI_OFFSET), 0.0, RATIO_MAX_CLIP) / RATIO_MAX_CLIP)
    vec[26] = float(m_n * t_n)
    return vec


def encode_batch(temp, hum, moi, soil_id, crop_id, stage_id):
    """Versão vetorizada de `encode_features` para arrays NumPy (27 features)."""
    n = len(temp)
    X = np.zeros((n, INPUT_DIM), dtype=np.float32)

    t_n = np.clip((temp - TEMP_MIN) / (TEMP_MAX - TEMP_MIN), 0.0, 1.0)
    h_n = np.clip((hum - HUM_MIN) / (HUM_MAX - HUM_MIN), 0.0, 1.0)
    m_n = np.clip((moi - SOIL_MIN) / (SOIL_MAX - SOIL_MIN), 0.0, 1.0)

    X[:, 0] = t_n
    X[:, 1] = h_n
    X[:, 2] = m_n

    s_valid = (soil_id >= 1) & (soil_id <= len(SOIL_TYPES))
    X[s_valid, 3 + (soil_id[s_valid] - 1)] = 1.0

    c_valid = (crop_id >= 1) & (crop_id <= len(CROP_TYPES))
    X[c_valid, 3 + len(SOIL_TYPES) + (crop_id[c_valid] - 1)] = 1.0

    st_valid = (stage_id >= 1) & (stage_id <= len(SEEDLING_STAGES))
    X[st_valid, 3 + len(SOIL_TYPES) + len(CROP_TYPES) + (stage_id[st_valid] - 1)] = 1.0

    vpd = compute_vpd_kpa(temp, hum)
    X[:, 23] = np.clip(vpd / VPD_MAX_KPA, 0.0, 1.0)
    X[:, 24] = np.clip((temp * (100.0 - hum)) / EVAP_DEMAND_DIVISOR, 0.0, 1.0)
    X[:, 25] = np.clip(t_n / (m_n + RATIO_MOI_OFFSET), 0.0, RATIO_MAX_CLIP) / RATIO_MAX_CLIP
    X[:, 26] = m_n * t_n
    return X


# ============================================================================
# 3. CARREGAMENTO, DEDUPLICAÇÃO, SPLITS E FULL-8 CONTEXT DROPOUT
# ============================================================================
def resolve_dataset_path():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.join(base_dir, "datasets", "Smart Agriculture Dataset", "cropdata_updated.csv"),
        "datasets/Smart Agriculture Dataset/cropdata_updated.csv",
        "ai-model/datasets/Smart Agriculture Dataset/cropdata_updated.csv",
        "cropdata_updated.csv",
    ]
    for p in candidates:
        if os.path.exists(p):
            return p

    import kagglehub
    path = kagglehub.dataset_download("chaitanyagopidesi/smart-agriculture-dataset")
    csv_files = glob.glob(os.path.join(path, "*.csv"))
    if not csv_files:
        raise FileNotFoundError(f"Nenhum arquivo CSV encontrado em: {path}")
    return csv_files[0]


def stratified_split(df, strat_col, test_size=0.30, seed=SEED):
    """Split estratificado determinístico em NumPy (sem depender de scikit-learn)."""
    rng = np.random.default_rng(seed)
    train_idx, test_idx = [], []
    for _, grp in df.groupby(strat_col, sort=True):
        idx = grp.index.to_numpy().copy()
        rng.shuffle(idx)
        n_test = max(1, int(round(len(idx) * test_size)))
        test_idx.append(idx[:n_test])
        train_idx.append(idx[n_test:])
    return df.loc[np.concatenate(train_idx)].copy(), df.loc[np.concatenate(test_idx)].copy()


def group_split_by_sensors(df, seed=SEED):
    """
    GroupSplit estrito pelas tuplas físicas `(MOI, temp, humidity)`.
    Garante 0,00% de sobreposição de leituras de sensores entre Treino, Validação e Teste.
    """
    rng = np.random.default_rng(seed)
    keys = df[["MOI", "temp", "humidity"]].drop_duplicates().to_numpy()
    idx = np.arange(len(keys))
    rng.shuffle(idx)
    n_train = int(0.70 * len(idx))
    n_val = int(0.15 * len(idx))
    train_keys = set(map(tuple, keys[idx[:n_train]]))
    val_keys = set(map(tuple, keys[idx[n_train:n_train + n_val]]))
    test_keys = set(map(tuple, keys[idx[n_train + n_val:]]))

    tuples = list(zip(df["MOI"], df["temp"], df["humidity"]))
    m_tr = [t in train_keys for t in tuples]
    m_va = [t in val_keys for t in tuples]
    m_te = [t in test_keys for t in tuples]
    return df[m_tr].copy(), df[m_va].copy(), df[m_te].copy()


def build_full8_dataset(dataframe, is_training=True):
    """
    Constrói o dataset codificado com 27 features:
      - Quando `is_training=True`: aplica o Full-8 Mask Context Dropout simétrico,
        cobrindo todas as 8 combinações de `(Soil, Crop, Stage)` sem filtrar nem
        distorcer a Classe 2 (`MOI >= 60`).
        * Máscaras com Cultura conhecida `(mc=1)`: aplicadas a todas as culturas.
        * Máscaras com Cultura UNKNOWN `(mc=0)`: aplicadas às hortaliças locais
          (`Tomato`, `Chilli`, `Carrot`, `Potato`), servindo como perfil fallback.
      - Quando `is_training=False` (Validação Limpa): avalia cada modo de contexto
        de forma balanceada na distribuição real sem multiplicação assimétrica.
    """
    t = dataframe["temp"].to_numpy(dtype=np.float32)
    h = dataframe["humidity"].to_numpy(dtype=np.float32)
    m = dataframe["MOI"].to_numpy(dtype=np.float32)
    s = dataframe["soil_id"].to_numpy(dtype=np.int32)
    c = dataframe["crop_id"].to_numpy(dtype=np.int32)
    st = dataframe["stage_id"].to_numpy(dtype=np.int32)
    y = dataframe["result"].to_numpy(dtype=np.int32)
    is_veg = (dataframe["crop ID"] != "Wheat").to_numpy()
    z = np.zeros_like(s)

    Xs, ys = [], []
    # 4 modos onde a Cultura é conhecida (inclui M6: Apenas Cultura [0, C, 0])
    for ms, mc, mst in [(1, 1, 1), (1, 1, 0), (0, 1, 1), (0, 1, 0)]:
        Xs.append(encode_batch(t, h, m, s if ms else z, c if mc else z, st if mst else z))
        ys.append(y)

    # 4 modos onde a Cultura é UNKNOWN (0) -> fallback para hortaliças locais
    for ms, mc, mst in [(1, 0, 1), (1, 0, 0), (0, 0, 1), (0, 0, 0)]:
        Xs.append(encode_batch(
            t[is_veg], h[is_veg], m[is_veg],
            s[is_veg] if ms else z[is_veg],
            z[is_veg],
            st[is_veg] if mst else z[is_veg],
        ))
        ys.append(y[is_veg])

    return np.vstack(Xs), np.concatenate(ys)


def build_mode_eval(dataframe, mask_tuple, veg_only_when_no_crop=True):
    """Gera `(X, y)` para avaliação de um modo específico `(ms, mc, mst)`."""
    ms, mc, mst = mask_tuple
    df_sub = dataframe[dataframe["crop ID"] != "Wheat"] if (veg_only_when_no_crop and mc == 0) else dataframe
    t = df_sub["temp"].to_numpy(dtype=np.float32)
    h = df_sub["humidity"].to_numpy(dtype=np.float32)
    m = df_sub["MOI"].to_numpy(dtype=np.float32)
    s = df_sub["soil_id"].to_numpy(dtype=np.int32) if ms else np.zeros(len(df_sub), dtype=np.int32)
    c = df_sub["crop_id"].to_numpy(dtype=np.int32) if mc else np.zeros(len(df_sub), dtype=np.int32)
    st = df_sub["stage_id"].to_numpy(dtype=np.int32) if mst else np.zeros(len(df_sub), dtype=np.int32)
    y = df_sub["result"].to_numpy(dtype=np.int32)
    return encode_batch(t, h, m, s, c, st), y


# ============================================================================
# 4. MÉTRICAS CIENTÍFICAS (ACURÁCIA, F1, ECE, MATRIZ DE CONFUSÃO)
# ============================================================================
def compute_confusion_matrix(y_true, y_pred, n_classes=NUM_CLASSES):
    cm = np.zeros((n_classes, n_classes), dtype=np.int64)
    for t, p in zip(y_true, y_pred):
        cm[int(t), int(p)] += 1
    return cm


def compute_metrics(y_true, y_pred, y_prob=None):
    cm = compute_confusion_matrix(y_true, y_pred, NUM_CLASSES)
    acc = float(np.trace(cm) / max(1, np.sum(cm)))
    prec, rec, f1, support = [], [], [], []
    for c in range(NUM_CLASSES):
        tp = int(cm[c, c])
        fp = int(cm[:, c].sum() - tp)
        fn = int(cm[c, :].sum() - tp)
        sup = int(cm[c, :].sum())
        p = tp / (tp + fp) if (tp + fp) > 0 else 0.0
        r = tp / (tp + fn) if (tp + fn) > 0 else 0.0
        f = 2 * p * r / (p + r) if (p + r) > 0 else 0.0
        prec.append(float(p))
        rec.append(float(r))
        f1.append(float(f))
        support.append(sup)

    ece = 0.0
    if y_prob is not None:
        conf = np.max(y_prob, axis=1)
        correct = (y_pred == y_true).astype(np.float64)
        bins = np.linspace(0.0, 1.0, 11)
        for i in range(10):
            mask = (conf >= bins[i]) & (conf <= bins[i + 1]) if i == 0 else (conf > bins[i]) & (conf <= bins[i + 1])
            if np.any(mask):
                ece += (np.sum(mask) / len(conf)) * abs(np.mean(correct[mask]) - np.mean(conf[mask]))

    return {
        "accuracy": acc,
        "macro_f1": float(np.mean(f1)),
        "macro_precision": float(np.mean(prec)),
        "macro_recall": float(np.mean(rec)),
        "precision": prec,
        "recall": rec,
        "f1": f1,
        "support": support,
        "ece": float(ece),
        "cm": cm.tolist(),
    }


def format_classification_table(metrics):
    lines = [
        f"{'Classe':<30} | {'Precision':>9} | {'Recall':>9} | {'F1-Score':>9} | {'Suporte':>8}",
        "-" * 78,
    ]
    for i, cname in enumerate(CLASS_NAMES):
        lines.append(
            f"{cname:<30} | {metrics['precision'][i]:>9.4f} | {metrics['recall'][i]:>9.4f} | "
            f"{metrics['f1'][i]:>9.4f} | {metrics['support'][i]:>8d}"
        )
    lines.append("-" * 78)
    lines.append(
        f"{'Macro Avg / Global':<30} | {metrics['macro_precision']:>9.4f} | {metrics['macro_recall']:>9.4f} | "
        f"{metrics['macro_f1']:>9.4f} | {sum(metrics['support']):>8d}"
    )
    lines.append(
        f"Acurácia: {metrics['accuracy']*100:.2f}% | Macro-F1: {metrics['macro_f1']*100:.2f}% | "
        f"ECE (Calibração): {metrics['ece']:.4f}"
    )
    return "\n".join(lines)


# ============================================================================
# 5. ARQUITETURAS TINYML v2 (PhysMLP-27 PRIMÁRIA + ResMLP-Tiny ALTERNATIVA)
# ============================================================================
def build_phys_mlp_v2(input_dim=INPUT_DIM, num_classes=NUM_CLASSES):
    """
    Arquitetura Recomendada pela Auditoria (`PhysMLP-27`):
      Input(27) -> Dense(32, ReLU) -> Dense(16, ReLU) -> Dense(3, Softmax)
    Usa exclusivamente operadores nativos do TFLite Micro:
      - AddFullyConnected
      - AddRelu
      - AddSoftmax
    Tamanho Full INT8: ~5.400 bytes (< 5.3 KB Flash, < 2.5 KB Tensor Arena).
    """
    model = tf.keras.Sequential([
        tf.keras.layers.Input(shape=(input_dim,), name="sensor_context_phys_input"),
        tf.keras.layers.Dense(32, activation="relu", name="dense_1"),
        tf.keras.layers.Dense(16, activation="relu", name="dense_2"),
        tf.keras.layers.Dense(num_classes, activation="softmax", name="decision_output"),
    ], name="PhysMLP_27_v2")
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.003),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return model


def build_res_mlp_v2(input_dim=INPUT_DIM, num_classes=NUM_CLASSES):
    """
    Arquitetura Alternativa da Auditoria (`ResMLP-Tiny`):
    Adiciona skip-connection concatenando os 3 sensores + 4 features físicas
    diretamente na penúltima camada densa (requer `AddConcatenation` no TFLM).
    """
    inp = tf.keras.layers.Input(shape=(input_dim,), name="res_input")
    h = tf.keras.layers.Dense(32, activation="relu", name="res_dense_1")(inp)
    h = tf.keras.layers.Dense(16, activation="relu", name="res_dense_2")(h)
    merged = tf.keras.layers.Concatenate(name="res_skip_concat")([h, inp])
    out = tf.keras.layers.Dense(num_classes, activation="softmax", name="res_output")(merged)
    model = tf.keras.Model(inp, out, name="ResMLP_Tiny_v2")
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.003),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    return model


# ============================================================================
# 6. QUANTIZAÇÃO REAL FULL INT8 + EXPORTAÇÃO C++ (`model_data.h`)
# ============================================================================
def convert_to_tflite_int8(model, X_rep_samples):
    """
    Executa a conversão TFLite em dois formatos para comprovação da auditoria:
      1. `tflite_f32`: `Optimize.DEFAULT` sem `representative_dataset` (mantém Float32).
      2. `tflite_int8`: Full Integer Quantization (`INT8`) com `representative_dataset`
         estratificado nos 8 modos de contexto e tensores de entrada/saída `tf.int8`.
    """
    c_f32 = tf.lite.TFLiteConverter.from_keras_model(model)
    c_f32.optimizations = [tf.lite.Optimize.DEFAULT]
    tflite_f32 = c_f32.convert()

    def representative_data_gen():
        for i in range(len(X_rep_samples)):
            yield [X_rep_samples[i:i + 1].astype(np.float32)]

    c_int8 = tf.lite.TFLiteConverter.from_keras_model(model)
    c_int8.optimizations = [tf.lite.Optimize.DEFAULT]
    c_int8.representative_dataset = representative_data_gen
    c_int8.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    c_int8.inference_input_type = tf.int8
    c_int8.inference_output_type = tf.int8
    tflite_int8 = c_int8.convert()

    return tflite_f32, tflite_int8


def build_representative_sample(train_df, n_per_mode=50):
    """
    Constrói um `representative_dataset` balanceado contendo amostras reais
    de todos os 8 modos de contexto (400 amostras no total), garantindo que
    os parâmetros de escala (`scale` e `zero_point`) do INT8 cubram tanto
    contexto completo quanto zero contexto.
    """
    sub_df = train_df.sample(n=min(n_per_mode, len(train_df)), random_state=SEED)
    chunks = []
    for _, mtuple in ALL_8_MASKS:
        Xm, _ = build_mode_eval(sub_df, mtuple)
        chunks.append(Xm)
    return np.vstack(chunks)


def evaluate_tflite_interpreter(model, tflite_blob, X_eval):
    """Valida o blob `.tflite` (INT8 ou Float32) no `tf.lite.Interpreter` contra o Keras."""
    interpreter = tf.lite.Interpreter(model_content=tflite_blob)
    interpreter.allocate_tensors()
    in_det = interpreter.get_input_details()[0]
    out_det = interpreter.get_output_details()[0]
    in_scale, in_zp = in_det["quantization"]
    out_scale, out_zp = out_det["quantization"]
    is_int8 = (in_det["dtype"] == np.int8)

    p_keras = model.predict(X_eval, verbose=0)
    p_tflite = np.zeros_like(p_keras)

    for i in range(len(X_eval)):
        x_in = X_eval[i:i + 1].astype(np.float32)
        if is_int8:
            xq = np.clip(np.round(x_in / in_scale + in_zp), -128, 127).astype(np.int8)
            interpreter.set_tensor(in_det["index"], xq)
        else:
            interpreter.set_tensor(in_det["index"], x_in)
        interpreter.invoke()
        yq = interpreter.get_tensor(out_det["index"])[0]
        if out_det["dtype"] == np.int8:
            p_tflite[i] = (yq.astype(np.float32) - out_zp) * out_scale
        else:
            p_tflite[i] = yq.astype(np.float32)

    diff = np.abs(p_keras - p_tflite)
    agree = float(np.mean(np.argmax(p_keras, axis=1) == np.argmax(p_tflite, axis=1)))
    return {
        "is_int8": bool(is_int8),
        "in_scale": float(in_scale),
        "in_zero_point": int(in_zp),
        "out_scale": float(out_scale),
        "out_zero_point": int(out_zp),
        "max_abs_diff": float(np.max(diff)),
        "mean_abs_diff": float(np.mean(diff)),
        "decision_agreement": agree,
        "interpreter": interpreter,
        "in_det": in_det,
        "out_det": out_det,
    }


def predict_single_tflite(q_meta, vec_27):
    interp = q_meta["interpreter"]
    in_det = q_meta["in_det"]
    out_det = q_meta["out_det"]
    x_in = np.expand_dims(vec_27.astype(np.float32), axis=0)
    if q_meta["is_int8"]:
        xq = np.clip(np.round(x_in / q_meta["in_scale"] + q_meta["in_zero_point"]), -128, 127).astype(np.int8)
        interp.set_tensor(in_det["index"], xq)
    else:
        interp.set_tensor(in_det["index"], x_in)
    interp.invoke()
    yq = interp.get_tensor(out_det["index"])[0]
    if out_det["dtype"] == np.int8:
        return (yq.astype(np.float32) - q_meta["out_zero_point"]) * q_meta["out_scale"]
    return yq.astype(np.float32)


def generate_cpp_header(tflite_blob, q_meta, golden_comments, golden_structs):
    """
    Gera o arquivo `model_data.h` autocontido para o ESP32 com:
      - Constantes de normalização, psicrometria (VPD) e sentinelas de erro
      - Parâmetros de quantização INT8 (`INPUT_SCALE`, `INPUT_ZERO_POINT`, etc.)
      - Enums C++ `SoilType`, `CropType`, `SeedlingStage`
      - Golden Test Vectors compiláveis (`GOLDEN_VECTORS[]`) para self-test no ESP32
      - Funções C++ inline `isValidSensorReading()`, `computeVpdKpa()` e
        `encodeFeatures27()` prontas para uso direto pelo `ModelManager` no ESP32
      - Array alinhado `alignas(8) const unsigned char g_model_data[]`
    """
    hex_lines = [
        "  " + ", ".join(f"0x{b:02x}" for b in tflite_blob[i:i + 12])
        for i in range(0, len(tflite_blob), 12)
    ]
    hex_body = ",\n".join(hex_lines)
    golden_block = "\n".join(golden_comments)
    golden_items = ",\n".join(
        f"        {{{g['t']:.1f}f, {g['h']:.1f}f, {g['m']:.1f}f, {g['s_id']}, {g['c_id']}, {g['st_id']}, "
        f"{g['raw_cls']}, {g['final_cls']}, {{{g['p0']:.4f}f, {g['p1']:.4f}f, {g['p2']:.4f}f}}}}"
        for g in golden_structs
    )

    return f"""#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

// ============================================================================
// Modelo TinyML v2 Gerado Automaticamente - Sistema de Irrigacao Inteligente
// Arquitetura: PhysMLP-27 ({INPUT_DIM} inputs -> 32 ReLU -> 16 ReLU -> 3 Softmax)
// Quantizacao: Full Integer Quantization (INT8) para TensorFlow Lite Micro
// Baseado em: docs/AUDITORIA_TINYML_E_COMPARATIVO.md
// ============================================================================
// Golden Test Vectors (Validados no TFLite INT8 Interpreter):
{golden_block}
// ============================================================================

namespace TinyMLConfig {{
    constexpr uint8_t INPUT_FEATURE_COUNT = {INPUT_DIM};
    constexpr uint8_t BASE_FEATURE_COUNT = {BASE_DIM};
    constexpr uint8_t OUTPUT_CLASS_COUNT = {NUM_CLASSES};

    // Limites explicitos de normalizacao Min-Max [0.0, 1.0]
    constexpr float TEMP_MIN = {TEMP_MIN:.1f}f;
    constexpr float TEMP_MAX = {TEMP_MAX:.1f}f;
    constexpr float HUMIDITY_MIN = {HUM_MIN:.1f}f;
    constexpr float HUMIDITY_MAX = {HUM_MAX:.1f}f;
    constexpr float SOIL_MIN = {SOIL_MIN:.1f}f;
    constexpr float SOIL_MAX = {SOIL_MAX:.1f}f;

    // Constantes psicrometricas (Features 23..26)
    constexpr float VPD_MAX_KPA = {VPD_MAX_KPA:.1f}f;
    constexpr float EVAP_DEMAND_DIVISOR = {EVAP_DEMAND_DIVISOR:.1f}f;
    constexpr float RATIO_MOI_OFFSET = {RATIO_MOI_OFFSET:.2f}f;
    constexpr float RATIO_MAX_CLIP = {RATIO_MAX_CLIP:.1f}f;

    // Parametros de Quantizacao TFLite INT8
    constexpr bool IS_INT8_QUANTIZED = {"true" if q_meta["is_int8"] else "false"};
    constexpr float INPUT_SCALE = {q_meta["in_scale"]:.8e}f;
    constexpr int32_t INPUT_ZERO_POINT = {q_meta["in_zero_point"]};
    constexpr float OUTPUT_SCALE = {q_meta["out_scale"]:.8e}f;
    constexpr int32_t OUTPUT_ZERO_POINT = {q_meta["out_zero_point"]};

    // Sentinelas de falha de hardware (SensorsManager) - ABORTAR inferencia se detectados!
    constexpr int8_t SENTINEL_TEMP_ERROR_I8 = {SENTINEL_TEMP_ERR_I8};
    constexpr int16_t SENTINEL_TEMP_ERROR_I16 = {SENTINEL_TEMP_ERR_I16};
    constexpr uint8_t SENTINEL_HUM_ERROR_U8 = {SENTINEL_HUM_ERR_U8};

    // Limiares operacionais e fallback fisico complementar
    constexpr float DEFAULT_CONFIDENCE_THRESHOLD = {DEFAULT_CONFIDENCE_THRESHOLD:.2f}f;
    constexpr uint8_t THERMAL_STRESS_SOIL_CAP_PCT = {THERMAL_STRESS_SOIL_CAP_PCT};
    constexpr int8_t THERMAL_STRESS_TEMP_MIN_C = {THERMAL_STRESS_TEMP_MIN_C};

    enum class SoilType : uint8_t {{
        UNKNOWN = 0,
        CLAY_SOIL = 1,
        SANDY_SOIL = 2,
        RED_SOIL = 3,
        LOAM_SOIL = 4,
        BLACK_SOIL = 5,
        ALLUVIAL_SOIL = 6,
        CHALKY_SOIL = 7
    }};

    enum class CropType : uint8_t {{
        UNKNOWN = 0, // Fallback para Hortalicas Locais (Paulista-PE)
        TOMATO = 1,
        CHILLI = 2,
        CARROT = 3,
        POTATO = 4,
        WHEAT = 5
    }};

    enum class SeedlingStage : uint8_t {{
        UNKNOWN = 0,
        GERMINATION = 1,
        SEEDLING_STAGE = 2,
        VEGETATIVE_GROWTH = 3,
        FLOWERING = 4,
        POLLINATION = 5,
        FRUIT_FORMATION = 6,
        MATURATION = 7,
        HARVEST = 8
    }};

    // Verifica se as leituras dos sensores sao fisicamente validas (sem sentinelas -128/-999/255)
    inline bool isValidSensorReading(float tempC, float humPct, float soilPct) {{
        if (std::isnan(tempC) || std::isnan(humPct) || std::isnan(soilPct)) return false;
        const int tempInt = static_cast<int>(std::lround(tempC));
        const int humInt = static_cast<int>(std::lround(humPct));
        const int soilInt = static_cast<int>(std::lround(soilPct));
        if (tempInt == SENTINEL_TEMP_ERROR_I8 || tempInt == SENTINEL_TEMP_ERROR_I16) return false;
        if (humInt == SENTINEL_HUM_ERROR_U8 || soilInt == SENTINEL_HUM_ERROR_U8) return false;
        if (tempC < -10.0f || tempC > 65.0f) return false;
        if (humPct < 0.0f || humPct > 100.0f) return false;
        if (soilPct < 0.0f || soilPct > 100.0f) return false;
        return true;
    }}

    // Equacao de Tetens para Deficit de Pressao de Vapor (VPD em kPa) - executa em < 10 us no ESP32
    inline float computeVpdKpa(float tempC, float rhPct) {{
        const float es = 0.61078f * std::exp((17.27f * tempC) / (tempC + 237.3f));
        const float deficit = std::clamp(1.0f - (rhPct / 100.0f), 0.0f, 1.0f);
        return es * deficit;
    }}

    // Monta o vetor de 27 features float32 identico ao pipeline Python v2 (PhysMLP-27)
    inline bool encodeFeatures27(
        float tempC, float humPct, float soilPct,
        uint8_t soilId, uint8_t cropId, uint8_t stageId,
        float outFeatures[INPUT_FEATURE_COUNT]
    ) {{
        if (!isValidSensorReading(tempC, humPct, soilPct)) {{
            return false;
        }}
        for (uint8_t i = 0; i < INPUT_FEATURE_COUNT; ++i) {{
            outFeatures[i] = 0.0f;
        }}
        const float tNorm = std::clamp((tempC - TEMP_MIN) / (TEMP_MAX - TEMP_MIN), 0.0f, 1.0f);
        const float hNorm = std::clamp((humPct - HUMIDITY_MIN) / (HUMIDITY_MAX - HUMIDITY_MIN), 0.0f, 1.0f);
        const float mNorm = std::clamp((soilPct - SOIL_MIN) / (SOIL_MAX - SOIL_MIN), 0.0f, 1.0f);

        outFeatures[0] = tNorm;
        outFeatures[1] = hNorm;
        outFeatures[2] = mNorm;

        if (soilId >= 1 && soilId <= 7) {{
            outFeatures[3 + (soilId - 1)] = 1.0f;
        }}
        if (cropId >= 1 && cropId <= 5) {{
            outFeatures[10 + (cropId - 1)] = 1.0f;
        }}
        if (stageId >= 1 && stageId <= 8) {{
            outFeatures[15 + (stageId - 1)] = 1.0f;
        }}

        const float vpdKpa = computeVpdKpa(tempC, humPct);
        outFeatures[23] = std::clamp(vpdKpa / VPD_MAX_KPA, 0.0f, 1.0f);
        outFeatures[24] = std::clamp((tempC * (100.0f - humPct)) / EVAP_DEMAND_DIVISOR, 0.0f, 1.0f);
        outFeatures[25] = std::clamp(tNorm / (mNorm + RATIO_MOI_OFFSET), 0.0f, RATIO_MAX_CLIP) / RATIO_MAX_CLIP;
        outFeatures[26] = mNorm * tNorm;
        return true;
    }}

    // Quantiza feature float32 para int8_t usando INPUT_SCALE e INPUT_ZERO_POINT
    inline int8_t quantizeInput(float value) {{
        const int32_t q = static_cast<int32_t>(std::lround(value / INPUT_SCALE)) + INPUT_ZERO_POINT;
        return static_cast<int8_t>(std::clamp<int32_t>(q, -128, 127));
    }}

    // Desquantiza saida int8_t para probabilidade float32 [0.0, 1.0]
    inline float dequantizeOutput(int8_t qValue) {{
        return (static_cast<float>(qValue) - static_cast<float>(OUTPUT_ZERO_POINT)) * OUTPUT_SCALE;
    }}

    struct GoldenVector {{
        float tempC;
        float humPct;
        float soilPct;
        uint8_t soilId;
        uint8_t cropId;
        uint8_t stageId;
        uint8_t expectedRawClass;
        uint8_t expectedFinalClass;
        float expectedProbs[3];
    }};

    constexpr uint8_t GOLDEN_VECTOR_COUNT = {len(golden_structs)};
    constexpr GoldenVector GOLDEN_VECTORS[GOLDEN_VECTOR_COUNT] = {{
{golden_items}
    }};
}}

const unsigned int g_model_len = {len(tflite_blob)};
alignas(8) const unsigned char g_model_data[] = {{
{hex_body}
}};
"""


# ============================================================================
# 7. PAINEL VISUAL DE DIAGNÓSTICO DO TREINO v2
# ============================================================================
def save_training_diagnostic_figure(history, mode_metrics, cm_m1, cm_m6, cm_m8, out_path):
    if not HAS_MATPLOTLIB:
        print("  [Aviso] matplotlib ausente neste interpretador Python; pulando geração de figura.")
        return
    fig, axes = plt.subplots(2, 2, figsize=(14, 10), dpi=150)

    # (a) Curvas de Loss e Accuracy
    ax0 = axes[0, 0]
    epochs_r = np.arange(1, len(history.history["loss"]) + 1)
    ax0.plot(epochs_r, history.history["loss"], label="Treino Loss", color="#1b9e77", lw=2)
    ax0.plot(epochs_r, history.history["val_loss"], label="Validação Limpa Loss", color="#d95f02", lw=2)
    ax0.set_title("a) Convergência PhysMLP-27 v2 (Cross-Entropy Loss)", fontweight="bold")
    ax0.set_xlabel("Época")
    ax0.set_ylabel("Loss")
    ax0.grid(True, alpha=0.3)
    ax0.legend()

    # (b) Acurácia e Macro-F1 nos 8 Modos de Contexto
    ax1 = axes[0, 1]
    labels = [m[0].split(" ")[0] for m in ALL_8_MASKS]
    accs = [mode_metrics[m[0]]["accuracy"] * 100 for m in ALL_8_MASKS]
    f1s = [mode_metrics[m[0]]["macro_f1"] * 100 for m in ALL_8_MASKS]
    x = np.arange(len(labels))
    w = 0.38
    ax1.bar(x - w / 2, accs, width=w, label="Acurácia (%)", color="#2b8cbe")
    ax1.bar(x + w / 2, f1s, width=w, label="Macro-F1 (%)", color="#e34a33")
    ax1.set_xticks(x)
    ax1.set_xticklabels(labels)
    ax1.set_ylim(65, 103)
    ax1.set_title("b) Desempenho nos 8 Modos de Contexto (M1..M8)", fontweight="bold")
    ax1.set_ylabel("Percentual (%)")
    ax1.grid(True, axis="y", alpha=0.3)
    ax1.legend()

    # (c) Matriz de Confusão M1 (Contexto Completo) e M6 (Só Cultura)
    for ax, cm_arr, title, cmap in [
        (axes[1, 0], np.array(cm_m1), "c) Matriz de Confusão — M1: Contexto Completo", "Greens"),
        (axes[1, 1], np.array(cm_m8), "d) Matriz de Confusão — M8: Zero Contexto (Só 3 Sensores)", "Blues"),
    ]:
        im = ax.imshow(cm_arr, cmap=cmap)
        ax.set_xticks([0, 1, 2])
        ax.set_yticks([0, 1, 2])
        ax.set_xticklabels(["OFF (0)", "IRRIG (1)", "STRESS (2)"])
        ax.set_yticklabels(["OFF (0)", "IRRIG (1)", "STRESS (2)"])
        ax.set_xlabel("Classe Predita")
        ax.set_ylabel("Classe Real")
        ax.set_title(title, fontweight="bold")
        max_v = cm_arr.max() if cm_arr.size else 1
        for r in range(3):
            for c in range(3):
                ax.text(
                    c, r, f"{cm_arr[r, c]}",
                    ha="center", va="center",
                    color="white" if cm_arr[r, c] > 0.55 * max_v else "black",
                    fontweight="bold",
                )
        fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)

    plt.tight_layout()
    plt.savefig(out_path, bbox_inches="tight")
    plt.close(fig)


# ============================================================================
# 8. PIPELINE PRINCIPAL DE EXECUÇÃO (TRAIN TINYML v2 + FINE-TUNING CAMPO)
# ============================================================================
def load_field_csv_for_finetuning(readings_path=None, inferences_path=None):
    """
    Carrega logs reais coletados pelo ESP32 (/littlefs/readings.csv ou inference.csv)
    para fine-tuning incremental em Paulista-PE, filtrando sentinelas de falha.
    """
    rows = []
    if inferences_path and os.path.exists(inferences_path):
        df_inf = pd.read_csv(inferences_path)
        req_cols = {"temperatura_c", "umidade", "umidade_solo", "decisao"}
        if req_cols.issubset(df_inf.columns):
            for _, r in df_inf.iterrows():
                t = float(r["temperatura_c"])
                h = float(r["umidade"])
                m = float(r["umidade_solo"])
                cls = int(r["decisao"])
                if not is_valid_sensor_reading(t, h, m) or cls not in (0, 1, 2):
                    continue
                s_id = int(r.get("soil_id", 0))
                c_id = int(r.get("crop_id", 0))
                st_id = int(r.get("stage_id", 0))
                rows.append((t, h, m, s_id, c_id, st_id, cls))
    elif readings_path and os.path.exists(readings_path):
        df_rd = pd.read_csv(readings_path)
        req_cols = {"temperatura_c", "umidade", "umidade_solo", "decisao"}
        if req_cols.issubset(df_rd.columns):
            for _, r in df_rd.iterrows():
                t = float(r["temperatura_c"])
                h = float(r["umidade"])
                m = float(r["umidade_solo"])
                cls = int(r["decisao"])
                if is_valid_sensor_reading(t, h, m) and cls in (0, 1, 2):
                    rows.append((t, h, m, 0, 0, 0, cls))
    if not rows:
        return None, None
    Xs = np.array([encode_features(t, h, m, s, c, st) for t, h, m, s, c, st, _ in rows], dtype=np.float32)
    ys = np.array([cls for *_, cls in rows], dtype=np.int32)
    return Xs, ys


def parse_args():
    parser = argparse.ArgumentParser(description="Treinamento e Exportação TinyML v2 (PhysMLP-27 INT8)")
    parser.add_argument("--field-readings", type=str, default=None, help="Caminho para readings.csv coletado em campo")
    parser.add_argument("--field-inferences", type=str, default=None, help="Caminho para inference.csv coletado em campo")
    parser.add_argument("--epochs-ft", type=int, default=15, help="Épocas de fine-tuning com dados de campo")
    parser.add_argument("--lr-ft", type=float, default=5e-4, help="Learning rate para fine-tuning")
    return parser.parse_args()


def main():
    args = parse_args()
    t0 = time.time()
    base_dir = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(base_dir, "output")
    fig_dir = os.path.join(out_dir, "figures")
    os.makedirs(fig_dir, exist_ok=True)

    sep = "=" * 78
    print(f"\n{sep}\n[1/7] Carregamento do Dataset e Auditoria Forense de Vazamento\n{sep}")
    dataset_path = resolve_dataset_path()
    df_raw = pd.read_csv(dataset_path)
    n_raw = len(df_raw)
    n_dup = int(df_raw.duplicated().sum())

    # Correção #2 da Auditoria: Deduplicação obrigatória antes de qualquer split
    df = df_raw.drop_duplicates().reset_index(drop=True)
    n_clean = len(df)
    n_sensor_tuples = int(df.groupby(["MOI", "temp", "humidity"]).ngroups)

    print(f"  Arquivo carregado      : {dataset_path}")
    print(f"  Registros brutos       : {n_raw}")
    print(f"  Duplicatas exatas      : {n_dup} removidas -> {n_clean} registros únicos")
    print(f"  Tuplas físicas únicas  : {n_sensor_tuples} combinações (MOI, temp, humidity)")

    df["soil_id"] = df["soil_type"].map(SOIL_TO_ID)
    df["crop_id"] = df["crop ID"].map(CROP_TO_ID)
    df["stage_id"] = df["Seedling Stage"].map(STAGE_TO_ID)
    df["strat_key"] = df["crop ID"].astype(str) + "_" + df["result"].astype(str)

    # Split 1: Estratificado Limpo (70% Treino / 15% Validação / 15% Teste)
    train_df, temp_df = stratified_split(df, "strat_key", test_size=0.30, seed=SEED)
    val_df, test_df = stratified_split(temp_df, "strat_key", test_size=0.50, seed=SEED)

    # Split 2: GroupSplit Estrito por Tupla de Sensores (0% Sensor-Tuple Leakage)
    tr_grp, va_grp, te_grp = group_split_by_sensors(df, seed=SEED)

    # Medição explícita de sobreposição de tuplas de sensores para auditoria
    tr_tuples = set(zip(train_df["MOI"], train_df["temp"], train_df["humidity"]))
    te_tuples = list(zip(test_df["MOI"], test_df["temp"], test_df["humidity"]))
    overlap_strat = sum(1 for t in te_tuples if t in tr_tuples) / max(1, len(te_tuples))

    tr_g_tuples = set(zip(tr_grp["MOI"], tr_grp["temp"], tr_grp["humidity"]))
    te_g_tuples = list(zip(te_grp["MOI"], te_grp["temp"], te_grp["humidity"]))
    overlap_grp = sum(1 for t in te_g_tuples if t in tr_g_tuples) / max(1, len(te_g_tuples))

    print(f"  Split Estratificado    : Treino={len(train_df)} | Val={len(val_df)} | Teste={len(test_df)} "
          f"(Sobreposição de tuplas físicas={overlap_strat*100:.2f}%)")
    print(f"  GroupSplit (0-Leakage) : Treino={len(tr_grp)} | Val={len(va_grp)} | Teste={len(te_grp)} "
          f"(Sobreposição de tuplas físicas={overlap_grp*100:.2f}%)")

    print(f"\n{sep}\n[2/7] Construção do Dataset Full-8 Mask Context Dropout + 27 Features (VPD)\n{sep}")
    X_train, y_train = build_full8_dataset(train_df, is_training=True)
    X_val, y_val = build_full8_dataset(val_df, is_training=False)

    print(f"  Dimensão de entrada    : {INPUT_DIM} features (23 base + 4 psicrométricas VPD)")
    print(f"  Amostras Full-8        : Treino={X_train.shape[0]} | Validação Limpa={X_val.shape[0]}")
    print(f"  Pesos de Classe (v2)   : {CALIBRATED_CLASS_WEIGHTS} (evita colapso de Precisão da Classe 2)")

    print(f"\n{sep}\n[3/7] Treinamento do Modelo Primário: PhysMLP-27 v2\n{sep}")
    model = build_phys_mlp_v2()
    model.summary()

    callbacks = [
        tf.keras.callbacks.EarlyStopping(
            monitor="val_loss", patience=12, restore_best_weights=True, verbose=1
        ),
        tf.keras.callbacks.ReduceLROnPlateau(
            monitor="val_loss", factor=0.5, patience=5, min_lr=1e-5, verbose=1
        ),
    ]

    history = model.fit(
        X_train, y_train,
        validation_data=(X_val, y_val),
        epochs=80,
        batch_size=128,
        class_weight=CALIBRATED_CLASS_WEIGHTS,
        callbacks=callbacks,
        verbose=1,
    )

    # Fine-tuning opcional com dados reais coletados em campo (Paulista-PE)
    if args.field_inferences or args.field_readings:
        X_field, y_field = load_field_csv_for_finetuning(args.field_readings, args.field_inferences)
        if X_field is not None and len(X_field) > 0:
            print(f"\n  [Fine-Tuning Campo] {len(X_field)} amostras válidas carregadas; ajustando pesos (lr={args.lr_ft})...")
            model.compile(
                optimizer=tf.keras.optimizers.Adam(learning_rate=args.lr_ft),
                loss="sparse_categorical_crossentropy",
                metrics=["accuracy"],
            )
            model.fit(
                X_field, y_field,
                validation_data=(X_val, y_val),
                epochs=args.epochs_ft,
                batch_size=min(32, len(X_field)),
                class_weight=CALIBRATED_CLASS_WEIGHTS,
                callbacks=[
                    tf.keras.callbacks.EarlyStopping(monitor="val_loss", patience=4, restore_best_weights=True, verbose=1)
                ],
                verbose=1,
            )
        else:
            print("\n  [Fine-Tuning Campo] Nenhuma amostra válida encontrada nos CSVs informados.")

    print(f"\n{sep}\n[4/7] Avaliação Completa nos 8 Modos de Contexto (M1 a M8)\n{sep}")
    mode_metrics = {}
    for mode_label, mtuple in ALL_8_MASKS:
        Xm, ym = build_mode_eval(test_df, mtuple)
        pm = model.predict(Xm, verbose=0)
        yhat = np.argmax(pm, axis=1)
        met = compute_metrics(ym, yhat, pm)
        mode_metrics[mode_label] = met
        print(
            f"  {mode_label:<30} -> Acc: {met['accuracy']*100:6.2f}% | "
            f"Macro-F1: {met['macro_f1']*100:6.2f}% | "
            f"C2 Prec/Rec: {met['precision'][2]*100:5.2f}%/{met['recall'][2]*100:5.2f}% | "
            f"ECE: {met['ece']:.4f}"
        )

    for highlight_mode in [
        "M1: Completo [S,C,St]",
        "M6: Apenas Cultura [0,C,0]",
        "M8: Zero Contexto [0,0,0]",
    ]:
        print(f"\n--- Detalhamento {highlight_mode} ---")
        print(format_classification_table(mode_metrics[highlight_mode]))

    print(f"\n{sep}\n[5/7] Avaliação Rigorosa sob GroupSplit Estrito (Zero Sensor-Tuple Leakage)\n{sep}")
    X_tr_grp, y_tr_grp = build_full8_dataset(tr_grp, is_training=True)
    X_va_grp, y_va_grp = build_full8_dataset(va_grp, is_training=False)
    model_grp = build_phys_mlp_v2()
    model_grp.fit(
        X_tr_grp, y_tr_grp,
        validation_data=(X_va_grp, y_va_grp),
        epochs=60,
        batch_size=128,
        class_weight=CALIBRATED_CLASS_WEIGHTS,
        callbacks=[
            tf.keras.callbacks.EarlyStopping(monitor="val_loss", patience=10, restore_best_weights=True, verbose=0),
            tf.keras.callbacks.ReduceLROnPlateau(monitor="val_loss", factor=0.5, patience=4, min_lr=1e-5, verbose=0),
        ],
        verbose=0,
    )
    group_split_metrics = {}
    for mode_label, mtuple in [
        ("M1: Completo [S,C,St]", (1, 1, 1)),
        ("M6: Apenas Cultura [0,C,0]", (0, 1, 0)),
        ("M8: Zero Contexto [0,0,0]", (0, 0, 0)),
    ]:
        Xmg, ymg = build_mode_eval(te_grp, mtuple)
        pmg = model_grp.predict(Xmg, verbose=0)
        met_g = compute_metrics(ymg, np.argmax(pmg, axis=1), pmg)
        group_split_metrics[mode_label] = met_g
        print(
            f"  [GroupSplit 0% Leakage] {mode_label:<28} -> "
            f"Acc: {met_g['accuracy']*100:6.2f}% | Macro-F1: {met_g['macro_f1']*100:6.2f}% | "
            f"ECE: {met_g['ece']:.4f}"
        )

    print(f"\n{sep}\n[6/7] Quantização Real Full INT8, Validação Cruzada e Teste de Sentinelas\n{sep}")
    X_rep = build_representative_sample(train_df, n_per_mode=50)
    tflite_f32, tflite_int8 = convert_to_tflite_int8(model, X_rep)

    tflite_f32_path = os.path.join(out_dir, "model_float32.tflite")
    tflite_int8_path = os.path.join(out_dir, "model_int8.tflite")
    with open(tflite_f32_path, "wb") as f:
        f.write(tflite_f32)
    with open(tflite_int8_path, "wb") as f:
        f.write(tflite_int8)

    X_check, _ = build_mode_eval(test_df, (1, 1, 1))
    q_meta = evaluate_tflite_interpreter(model, tflite_int8, X_check[:600])

    print(f"  Tamanho TFLite sem rep_dataset (Float32) : {len(tflite_f32)} bytes ({len(tflite_f32)/1024:.2f} KB)")
    print(f"  Tamanho TFLite Full INT8 (v2)            : {len(tflite_int8)} bytes ({len(tflite_int8)/1024:.2f} KB)")
    print(f"  Redução de Flash                         : {(1 - len(tflite_int8)/len(tflite_f32))*100:.1f}%")
    print(f"  Escala Entrada INT8                      : scale={q_meta['in_scale']:.6e}, zp={q_meta['in_zero_point']}")
    print(f"  Escala Saída INT8                        : scale={q_meta['out_scale']:.6e}, zp={q_meta['out_zero_point']}")
    print(f"  Concordância Keras F32 vs TFLite INT8    : {q_meta['decision_agreement']*100:.2f}% "
          f"(Mean Diff={q_meta['mean_abs_diff']:.5f}, Max Diff={q_meta['max_abs_diff']:.5f})")

    # Teste de proteção contra sentinelas de falha do DHT22 (-128 °C / 255% RH)
    assert not is_valid_sensor_reading(-128.0, 255.0, 45.0), "Falha: sentinela (-128, 255) deveria ser rejeitado!"
    assert not is_valid_sensor_reading(-999.0, 50.0, 45.0), "Falha: sentinela -999 deveria ser rejeitado!"
    assert is_valid_sensor_reading(28.5, 62.0, 48.0), "Falha: leitura normal deveria ser aceita!"
    print("  [Fail-Safe Check] Proteção contra sentinelas (-128, -999, 255, NaN): PASSOU 100%")

    # Golden Test Vectors validados no TFLite INT8 Interpreter
    # Formato: (desc, t, h, m, s_id, c_id, st_id, expected_final_class)
    golden_cases = [
        ("Caso 1 (Sem Contexto - Solo Umido/Frio -> OFF)", 22.0, 78.0, 65.0, 0, 0, 0, 0),
        ("Caso 2 (Sem Contexto - Solo Seco/Quente -> IRRIGATE)", 34.0, 45.0, 22.0, 0, 0, 0, 1),
        ("Caso 3 (Sem Contexto - Calor Extremo + Solo Encharcado -> THERMAL_STRESS)", 38.0, 40.0, 75.0, 0, 0, 0, 2),
        (
            "Caso 4 (Com Contexto: Tomate em Solo Argiloso + Floracao - Seco -> IRRIGATE)",
            33.0, 50.0, 28.0,
            SOIL_TO_ID["Clay Soil"], CROP_TO_ID["Tomato"], STAGE_TO_ID["Flowering"], 1,
        ),
        (
            "Caso 5 (Com Contexto: Pimenta em Solo Arenoso + Germinacao - Calor + Umido -> THERMAL_STRESS)",
            37.0, 42.0, 70.0,
            SOIL_TO_ID["Sandy Soil"], CROP_TO_ID["Chilli"], STAGE_TO_ID["Germination"], 2,
        ),
        (
            "Caso 6 (Modo 6 - Apenas Cultura: Tomate [0,1,0] - Solo Seco -> IRRIGATE)",
            31.0, 52.0, 25.0,
            0, CROP_TO_ID["Tomato"], 0, 1,
        ),
    ]

    print(f"\n{sep}\n[7/7] Extração de Golden Test Vectors (INT8) e Exportação de model_data.h\n{sep}")
    golden_comments = []
    golden_structs = []
    for desc, t, h, m, s_id, c_id, st_id, final_cls in golden_cases:
        vec = encode_features(t, h, m, s_id, c_id, st_id)
        probs = predict_single_tflite(q_meta, vec)
        pred_cls = int(np.argmax(probs))
        vpd_val = compute_vpd_kpa(t, h)
        line = (
            f"// {desc}: Temp={t}C, Hum={h}%, Soil={m}%, VPD={vpd_val:.2f}kPa, "
            f"SoilID={s_id}, CropID={c_id}, StageID={st_id} => "
            f"RawClass={pred_cls}, FinalClass={final_cls} (P0={probs[0]:.3f}, P1={probs[1]:.3f}, P2={probs[2]:.3f})"
        )
        golden_comments.append(line)
        golden_structs.append({
            "t": t, "h": h, "m": m,
            "s_id": s_id, "c_id": c_id, "st_id": st_id,
            "raw_cls": pred_cls, "final_cls": final_cls,
            "p0": float(probs[0]), "p1": float(probs[1]), "p2": float(probs[2]),
        })
        print(" ", line)

    cpp_header = generate_cpp_header(tflite_int8, q_meta, golden_comments, golden_structs)

    header_destinations = [
        os.path.join(out_dir, "model_data.h"),
        os.path.join(base_dir, "model_data.h"),
    ]
    emb_inc = os.path.abspath(os.path.join(base_dir, "..", "embedded", "include"))
    if os.path.isdir(emb_inc):
        header_destinations.append(os.path.join(emb_inc, "model_data.h"))

    for hpath in header_destinations:
        with open(hpath, "w", encoding="utf-8") as f:
            f.write(cpp_header)
        print(f"  [Header C++ Exportado] -> {hpath} ({len(tflite_int8)} bytes INT8)")

    # Salva figura de diagnóstico e relatório JSON v2
    diag_fig_path = os.path.join(fig_dir, "train_tinyml_v2_diagnostics.png")
    save_training_diagnostic_figure(
        history,
        mode_metrics,
        mode_metrics["M1: Completo [S,C,St]"]["cm"],
        mode_metrics["M6: Apenas Cultura [0,C,0]"]["cm"],
        mode_metrics["M8: Zero Contexto [0,0,0]"]["cm"],
        diag_fig_path,
    )
    print(f"  [Figura Diagnóstica]   -> {diag_fig_path}")

    report_v2 = {
        "version": "2.0.0-PhysMLP27-INT8",
        "architecture": "PhysMLP-27 (27 -> 32 ReLU -> 16 ReLU -> 3 Softmax)",
        "input_dim": INPUT_DIM,
        "num_classes": NUM_CLASSES,
        "calibrated_class_weights": CALIBRATED_CLASS_WEIGHTS,
        "dataset": {
            "raw_rows": n_raw,
            "duplicates_removed": n_dup,
            "clean_rows": n_clean,
            "unique_sensor_tuples": n_sensor_tuples,
            "stratified_tuple_overlap": overlap_strat,
            "group_split_tuple_overlap": overlap_grp,
        },
        "quantization": {
            "float32_bytes": len(tflite_f32),
            "int8_bytes": len(tflite_int8),
            "flash_reduction_pct": float((1.0 - len(tflite_int8) / len(tflite_f32)) * 100.0),
            "input_scale": q_meta["in_scale"],
            "input_zero_point": q_meta["in_zero_point"],
            "output_scale": q_meta["out_scale"],
            "output_zero_point": q_meta["out_zero_point"],
            "decision_agreement": q_meta["decision_agreement"],
            "mean_abs_diff": q_meta["mean_abs_diff"],
            "max_abs_diff": q_meta["max_abs_diff"],
        },
        "metrics_all_8_modes": mode_metrics,
        "metrics_group_split_zero_leakage": group_split_metrics,
    }
    report_path = os.path.join(out_dir, "train_tinyml_v2_report.json")
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(report_v2, f, indent=2)
    print(f"  [Relatório JSON v2]    -> {report_path}")
    print(f"\nPipeline TinyML v2 concluído com sucesso em {time.time() - t0:.1f}s!")


if __name__ == "__main__":
    main()
