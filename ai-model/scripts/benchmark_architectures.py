#!/usr/bin/env python3
"""
Benchmark Científico Completo: Auditoria de Bugs, Arquiteturas Alternativas TinyML,
Quantização Real INT8 e Simulação ESP32.
Execução: /usr/bin/python3 ai-model/benchmark_architectures.py
"""

import os
import json
import time
import random
import numpy as np
import pandas as pd
import tensorflow as tf

os.environ["TF_CPP_MIN_LOG_LEVEL"] = "2"

SEED = 42
random.seed(SEED)
np.random.seed(SEED)
tf.random.set_seed(SEED)

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(BASE_DIR, "output")
os.makedirs(OUT_DIR, exist_ok=True)

DATA_PATH = os.path.join(
    BASE_DIR,
    "datasets",
    "Smart Agriculture Dataset",
    "cropdata_updated.csv",
)

TEMP_MIN, TEMP_MAX = 0.0, 50.0
HUM_MIN, HUM_MAX = 0.0, 100.0
SOIL_MIN, SOIL_MAX = 0.0, 100.0

SOIL_TYPES = [
    "Clay Soil", "Sandy Soil", "Red Soil", "Loam Soil",
    "Black Soil", "Alluvial Soil", "Chalky Soil"
]
CROP_TYPES = ["Tomato", "Chilli", "Carrot", "Potato", "Wheat"]
SEEDLING_STAGES = [
    "Germination", "Seedling Stage",
    "Vegetative Growth / Root or Tuber Development",
    "Flowering", "Pollination", "Fruit/Grain/Bulb Formation",
    "Maturation", "Harvest"
]

SOIL_TO_ID = {name: i + 1 for i, name in enumerate(SOIL_TYPES)}
CROP_TO_ID = {name: i + 1 for i, name in enumerate(CROP_TYPES)}
STAGE_TO_ID = {name: i + 1 for i, name in enumerate(SEEDLING_STAGES)}

BASE_DIM = 3 + len(SOIL_TYPES) + len(CROP_TYPES) + len(SEEDLING_STAGES)  # 23
PHYS_DIM = BASE_DIM + 4  # 27
NUM_CLASSES = 3


# ============================================================================
# 1. MÉTRICAS E UTILITÁRIOS NUMPY (SEM DEPENDÊNCIA DE SCIKIT-LEARN)
# ============================================================================
def compute_confusion_matrix(y_true, y_pred, n_classes=3):
    cm = np.zeros((n_classes, n_classes), dtype=np.int64)
    for t, p in zip(y_true, y_pred):
        cm[int(t), int(p)] += 1
    return cm


def compute_metrics(y_true, y_pred, y_prob=None):
    cm = compute_confusion_matrix(y_true, y_pred, NUM_CLASSES)
    acc = float(np.trace(cm) / max(1, np.sum(cm)))
    prec, rec, f1 = [], [], []
    for c in range(NUM_CLASSES):
        tp = cm[c, c]
        fp = cm[:, c].sum() - tp
        fn = cm[c, :].sum() - tp
        p = tp / (tp + fp) if (tp + fp) > 0 else 0.0
        r = tp / (tp + fn) if (tp + fn) > 0 else 0.0
        f = 2 * p * r / (p + r) if (p + r) > 0 else 0.0
        prec.append(float(p))
        rec.append(float(r))
        f1.append(float(f))
    out = {
        "accuracy": acc,
        "macro_f1": float(np.mean(f1)),
        "macro_precision": float(np.mean(prec)),
        "macro_recall": float(np.mean(rec)),
        "precision": prec,
        "recall": rec,
        "f1": f1,
        "cm": cm.tolist(),
    }
    if y_prob is not None:
        # ECE (Expected Calibration Error) with 10 bins
        conf = np.max(y_prob, axis=1)
        correct = (y_pred == y_true).astype(np.float64)
        bins = np.linspace(0.0, 1.0, 11)
        ece = 0.0
        for i in range(10):
            if i == 0:
                mask = (conf >= bins[i]) & (conf <= bins[i + 1])
            else:
                mask = (conf > bins[i]) & (conf <= bins[i + 1])
            if np.any(mask):
                ece += (np.sum(mask) / len(conf)) * abs(np.mean(correct[mask]) - np.mean(conf[mask]))
        out["ece"] = float(ece)
    return out


def compute_roc_pr_curves(y_true, y_prob, cls):
    y_bin = (y_true == cls).astype(np.int32)
    scores = y_prob[:, cls]
    order = np.argsort(-scores)
    y_sorted = y_bin[order]
    tp = np.cumsum(y_sorted)
    fp = np.cumsum(1 - y_sorted)
    pos = max(1, int(np.sum(y_bin)))
    neg = max(1, int(len(y_bin) - pos))
    tpr = np.concatenate([[0.0], tp / pos])
    fpr = np.concatenate([[0.0], fp / neg])
    roc_auc = float(np.trapezoid(tpr, fpr))

    prec = tp / np.maximum(1, tp + fp)
    rec = tp / pos
    prec = np.concatenate([[1.0], prec])
    rec = np.concatenate([[0.0], rec])
    auprc = float(np.trapezoid(prec, rec))
    return fpr, tpr, roc_auc, rec, prec, auprc


def stratified_split(df, strat_col, test_size=0.30, seed=SEED):
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


# ============================================================================
# 2. CODIFICAÇÃO DE FEATURES E ENGENHARIA PSICROMÉTRICA
# ============================================================================
def compute_vpd_kpa(temp_c, rh_pct):
    es = 0.61078 * np.exp((17.27 * temp_c) / (temp_c + 237.3))
    return es * np.clip(1.0 - rh_pct / 100.0, 0.0, 1.0)


def encode_batch(temp, hum, moi, soil_id, crop_id, stage_id, with_physics=False):
    n = len(temp)
    dim = PHYS_DIM if with_physics else BASE_DIM
    X = np.zeros((n, dim), dtype=np.float32)

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

    if with_physics:
        vpd = compute_vpd_kpa(temp, hum)
        X[:, 23] = np.clip(vpd / 8.0, 0.0, 1.0)                 # VPD normalizado (0..8 kPa)
        X[:, 24] = np.clip((temp * (100.0 - hum)) / 4000.0, 0.0, 1.0)  # Índice de demanda evaporativa
        X[:, 25] = np.clip(t_n / (m_n + 0.10), 0.0, 5.0) / 5.0  # Razão calor / umidade do solo
        X[:, 26] = m_n * t_n                                     # Acoplamento Solo Úmido x Calor (Classe 2)

    return X


def build_original_buggy_dataset(dataframe, is_training=True):
    t = dataframe["temp"].to_numpy(dtype=np.float32)
    h = dataframe["humidity"].to_numpy(dtype=np.float32)
    m = dataframe["MOI"].to_numpy(dtype=np.float32)
    s = dataframe["soil_id"].to_numpy(dtype=np.int32)
    c = dataframe["crop_id"].to_numpy(dtype=np.int32)
    st = dataframe["stage_id"].to_numpy(dtype=np.int32)
    y = dataframe["result"].to_numpy(dtype=np.int32)
    is_veg = (dataframe["crop ID"] != "Wheat").to_numpy()

    Xs, ys = [encode_batch(t, h, m, s, c, st)], [y]
    if is_training:
        z = np.zeros_like(s)
        Xs.append(encode_batch(t, h, m, s, c, z))
        ys.append(y)

        Xs.append(encode_batch(t[is_veg], h[is_veg], m[is_veg], s[is_veg], z[is_veg], z[is_veg]))
        ys.append(y[is_veg])

        mask4 = is_veg & ((y != 2) | (m >= 60.0))
        Xs.append(encode_batch(t[mask4], h[mask4], m[mask4], z[mask4], z[mask4], z[mask4]))
        ys.append(y[mask4])

    return np.vstack(Xs), np.concatenate(ys)


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


def build_clean_full8_dataset(dataframe, with_physics=False, include_dropouts=True):
    t = dataframe["temp"].to_numpy(dtype=np.float32)
    h = dataframe["humidity"].to_numpy(dtype=np.float32)
    m = dataframe["MOI"].to_numpy(dtype=np.float32)
    s = dataframe["soil_id"].to_numpy(dtype=np.int32)
    c = dataframe["crop_id"].to_numpy(dtype=np.int32)
    st = dataframe["stage_id"].to_numpy(dtype=np.int32)
    y = dataframe["result"].to_numpy(dtype=np.int32)
    is_veg = (dataframe["crop ID"] != "Wheat").to_numpy()
    z = np.zeros_like(s)

    Xs, ys = [encode_batch(t, h, m, s, c, st, with_physics)], [y]
    if include_dropouts:
        # Quando Cultura é conhecida, incluímos todas as culturas (inclusive Wheat)
        for ms, mc, mst in [(1, 1, 0), (0, 1, 1), (0, 1, 0)]:
            Xs.append(encode_batch(
                t, h, m,
                s if ms else z, c if mc else z, st if mst else z,
                with_physics
            ))
            ys.append(y)
        # Quando Cultura é UNKNOWN (0), usamos apenas hortaliças (fallback Paulista-PE)
        for ms, mc, mst in [(1, 0, 1), (1, 0, 0), (0, 0, 1), (0, 0, 0)]:
            Xs.append(encode_batch(
                t[is_veg], h[is_veg], m[is_veg],
                s[is_veg] if ms else z[is_veg],
                z[is_veg],
                st[is_veg] if mst else z[is_veg],
                with_physics
            ))
            ys.append(y[is_veg])

    return np.vstack(Xs), np.concatenate(ys)


def build_mode_eval(dataframe, mask_tuple, with_physics=False, veg_only_when_no_crop=True):
    ms, mc, mst = mask_tuple
    df_sub = dataframe[dataframe["crop ID"] != "Wheat"] if (veg_only_when_no_crop and mc == 0) else dataframe
    t = df_sub["temp"].to_numpy(dtype=np.float32)
    h = df_sub["humidity"].to_numpy(dtype=np.float32)
    m = df_sub["MOI"].to_numpy(dtype=np.float32)
    s = df_sub["soil_id"].to_numpy(dtype=np.int32) if ms else np.zeros(len(df_sub), dtype=np.int32)
    c = df_sub["crop_id"].to_numpy(dtype=np.int32) if mc else np.zeros(len(df_sub), dtype=np.int32)
    st = df_sub["stage_id"].to_numpy(dtype=np.int32) if mst else np.zeros(len(df_sub), dtype=np.int32)
    y = df_sub["result"].to_numpy(dtype=np.int32)
    return encode_batch(t, h, m, s, c, st, with_physics), y


# ============================================================================
# 3. IMPLEMENTAÇÃO VETORIZADA DE ÁRVORES (CART, RANDOM FOREST, GBDT)
# ============================================================================
class FastCARTNode:
    __slots__ = ("feat", "thresh", "left", "right", "value", "probs")

    def __init__(self, feat=None, thresh=None, left=None, right=None, value=None, probs=None):
        self.feat = feat
        self.thresh = thresh
        self.left = left
        self.right = right
        self.value = value
        self.probs = probs


class FastCARTClassifier:
    def __init__(self, max_depth=8, min_samples_leaf=8, max_features=None, seed=42):
        self.max_depth = max_depth
        self.min_samples_leaf = min_samples_leaf
        self.max_features = max_features
        self.rng = np.random.default_rng(seed)
        self.root = None
        self.n_nodes = 0

    def _fit_node(self, X, y, depth, mode="cls", sample_weight=None):
        self.n_nodes += 1
        if mode == "cls":
            counts = np.bincount(y, weights=sample_weight, minlength=NUM_CLASSES).astype(np.float64)
            probs = counts / max(1e-12, counts.sum())
            val = int(np.argmax(probs))
            if depth >= self.max_depth or len(y) < 2 * self.min_samples_leaf or np.max(probs) > 0.999:
                return FastCARTNode(value=val, probs=probs)
        else:
            val = float(np.mean(y))
            if depth >= self.max_depth or len(y) < 2 * self.min_samples_leaf or np.var(y) < 1e-7:
                return FastCARTNode(value=val)

        n_samples, n_feats = X.shape
        feat_idx = np.arange(n_feats)
        if self.max_features is not None and self.max_features < n_feats:
            feat_idx = self.rng.choice(n_feats, size=self.max_features, replace=False)

        best_gain = 1e-9
        best_feat, best_thresh = None, None

        if mode == "cls":
            parent_counts = np.bincount(y, weights=sample_weight, minlength=NUM_CLASSES).astype(np.float64)
            total_w = parent_counts.sum()
            parent_gini = 1.0 - np.sum((parent_counts / total_w) ** 2)

            for f in feat_idx:
                col = X[:, f]
                uniq = np.unique(col)
                if len(uniq) <= 1:
                    continue
                if len(uniq) > 24:
                    thresholds = np.quantile(col, np.linspace(0.05, 0.95, 19))
                else:
                    thresholds = 0.5 * (uniq[:-1] + uniq[1:])
                for th in thresholds:
                    lm = col <= th
                    nl = int(np.sum(lm))
                    nr = n_samples - nl
                    if nl < self.min_samples_leaf or nr < self.min_samples_leaf:
                        continue
                    wl = sample_weight[lm] if sample_weight is not None else None
                    wr = sample_weight[~lm] if sample_weight is not None else None
                    cl = np.bincount(y[lm], weights=wl, minlength=NUM_CLASSES).astype(np.float64)
                    cr = parent_counts - cl
                    sw_l = cl.sum()
                    sw_r = total_w - sw_l
                    if sw_l <= 0 or sw_r <= 0:
                        continue
                    gl = 1.0 - np.sum((cl / sw_l) ** 2)
                    gr = 1.0 - np.sum((cr / sw_r) ** 2)
                    gain = parent_gini - (sw_l / total_w) * gl - (sw_r / total_w) * gr
                    if gain > best_gain:
                        best_gain = gain
                        best_feat = int(f)
                        best_thresh = float(th)
        else:
            total_sum = np.sum(y)
            total_sq = np.sum(y * y)
            parent_mse = total_sq / n_samples - (total_sum / n_samples) ** 2
            for f in feat_idx:
                col = X[:, f]
                uniq = np.unique(col)
                if len(uniq) <= 1:
                    continue
                if len(uniq) > 20:
                    thresholds = np.quantile(col, np.linspace(0.06, 0.94, 15))
                else:
                    thresholds = 0.5 * (uniq[:-1] + uniq[1:])
                for th in thresholds:
                    lm = col <= th
                    nl = int(np.sum(lm))
                    nr = n_samples - nl
                    if nl < self.min_samples_leaf or nr < self.min_samples_leaf:
                        continue
                    yl = y[lm]
                    sl = float(np.sum(yl))
                    sr = total_sum - sl
                    gain = (sl * sl / nl + sr * sr / nr - total_sum * total_sum / n_samples) / n_samples
                    if gain > best_gain:
                        best_gain = gain
                        best_feat = int(f)
                        best_thresh = float(th)

        if best_feat is None:
            return FastCARTNode(value=val, probs=probs if mode == "cls" else None)

        lm = X[:, best_feat] <= best_thresh
        sw_l = sample_weight[lm] if sample_weight is not None else None
        sw_r = sample_weight[~lm] if sample_weight is not None else None
        left = self._fit_node(X[lm], y[lm], depth + 1, mode=mode, sample_weight=sw_l)
        right = self._fit_node(X[~lm], y[~lm], depth + 1, mode=mode, sample_weight=sw_r)
        return FastCARTNode(feat=best_feat, thresh=best_thresh, left=left, right=right, value=val)

    def fit(self, X, y, sample_weight=None):
        self.n_nodes = 0
        self.root = self._fit_node(X, y, 0, mode="cls", sample_weight=sample_weight)
        return self

    def _predict_batch(self, node, X, out, idx=None, is_probs=True):
        if idx is None:
            idx = np.arange(len(X))
        if len(idx) == 0:
            return
        if node.feat is None:
            out[idx] = node.probs if is_probs else node.value
            return
        lm = X[idx, node.feat] <= node.thresh
        if np.any(lm):
            self._predict_batch(node.left, X, out, idx[lm], is_probs)
        if np.any(~lm):
            self._predict_batch(node.right, X, out, idx[~lm], is_probs)

    def predict_proba(self, X):
        out = np.zeros((len(X), NUM_CLASSES), dtype=np.float32)
        self._predict_batch(self.root, X, out, is_probs=True)
        return out

    def predict(self, X):
        return np.argmax(self.predict_proba(X), axis=1)


class FastRandomForest:
    def __init__(self, n_trees=15, max_depth=8, min_samples_leaf=6, seed=42):
        self.n_trees = n_trees
        self.max_depth = max_depth
        self.min_samples_leaf = min_samples_leaf
        self.seed = seed
        self.trees = []

    def fit(self, X, y, sample_weight=None):
        rng = np.random.default_rng(self.seed)
        self.trees = []
        n = len(X)
        max_f = max(4, int(np.sqrt(X.shape[1]) * 1.5))
        for i in range(self.n_trees):
            idx = rng.integers(0, n, size=n)
            sw = sample_weight[idx] if sample_weight is not None else None
            tree = FastCARTClassifier(
                max_depth=self.max_depth,
                min_samples_leaf=self.min_samples_leaf,
                max_features=max_f,
                seed=self.seed + i
            )
            tree.fit(X[idx], y[idx], sample_weight=sw)
            self.trees.append(tree)
        return self

    def predict_proba(self, X):
        probs = np.zeros((len(X), NUM_CLASSES), dtype=np.float32)
        for t in self.trees:
            probs += t.predict_proba(X)
        return probs / len(self.trees)

    def predict(self, X):
        return np.argmax(self.predict_proba(X), axis=1)

    def total_nodes(self):
        return sum(t.n_nodes for t in self.trees)


class FastGBDTMulticlass:
    def __init__(self, n_estimators=35, lr=0.18, max_depth=4, min_samples_leaf=10, seed=42):
        self.n_estimators = n_estimators
        self.lr = lr
        self.max_depth = max_depth
        self.min_samples_leaf = min_samples_leaf
        self.seed = seed
        self.trees = []
        self.init_logits = None

    @staticmethod
    def _softmax(logits):
        e = np.exp(logits - np.max(logits, axis=1, keepdims=True))
        return e / np.sum(e, axis=1, keepdims=True)

    def fit(self, X, y):
        rng = np.random.default_rng(self.seed)
        n = len(X)
        priors = np.bincount(y, minlength=NUM_CLASSES).astype(np.float64) / n
        self.init_logits = np.log(np.clip(priors, 1e-6, 1.0))
        F = np.tile(self.init_logits, (n, 1))
        y_oh = np.eye(NUM_CLASSES, dtype=np.float64)[y]
        self.trees = []

        for m in range(self.n_estimators):
            probs = self._softmax(F)
            sub_idx = rng.choice(n, size=min(n, 16000), replace=False)
            round_trees = []
            for k in range(NUM_CLASSES):
                resid = y_oh[sub_idx, k] - probs[sub_idx, k]
                tree = FastCARTClassifier(
                    max_depth=self.max_depth,
                    min_samples_leaf=self.min_samples_leaf,
                    seed=self.seed + m * 10 + k
                )
                tree.n_nodes = 0
                tree.root = tree._fit_node(X[sub_idx], resid, 0, mode="reg")
                pred = np.zeros(n, dtype=np.float32)
                tree._predict_batch(tree.root, X, pred, is_probs=False)
                F[:, k] += self.lr * pred
                round_trees.append(tree)
            self.trees.append(round_trees)
        return self

    def predict_proba(self, X):
        F = np.tile(self.init_logits, (len(X), 1))
        for round_trees in self.trees:
            for k, tree in enumerate(round_trees):
                pred = np.zeros(len(X), dtype=np.float32)
                tree._predict_batch(tree.root, X, pred, is_probs=False)
                F[:, k] += self.lr * pred
        return self._softmax(F).astype(np.float32)

    def predict(self, X):
        return np.argmax(self.predict_proba(X), axis=1)

    def total_nodes(self):
        return sum(t.n_nodes for rt in self.trees for t in rt)


# ============================================================================
# 4. ARQUITETURAS NEURAIS TINYML (KERAS)
# ============================================================================
def make_mlp(input_dim, hidden_units, name, use_bn=False, dropout=0.0):
    layers = [tf.keras.layers.Input(shape=(input_dim,), name=f"{name}_in")]
    for i, u in enumerate(hidden_units):
        layers.append(tf.keras.layers.Dense(u, activation=None if use_bn else "relu", name=f"{name}_d{i}"))
        if use_bn:
            layers.append(tf.keras.layers.BatchNormalization(name=f"{name}_bn{i}"))
            layers.append(tf.keras.layers.ReLU(name=f"{name}_relu{i}"))
        if dropout > 0:
            layers.append(tf.keras.layers.Dropout(dropout, name=f"{name}_drop{i}"))
    layers.append(tf.keras.layers.Dense(NUM_CLASSES, activation="softmax", name=f"{name}_out"))
    return tf.keras.Sequential(layers, name=name)


def make_res_mlp(input_dim=BASE_DIM):
    inp = tf.keras.layers.Input(shape=(input_dim,), name="res_in")
    sensors = tf.keras.layers.Lambda(lambda x: x[:, :3], name="slice_sensors")(inp)
    h = tf.keras.layers.Dense(32, activation="relu", name="res_d1")(inp)
    h = tf.keras.layers.Dense(16, activation="relu", name="res_d2")(h)
    merged = tf.keras.layers.Concatenate(name="res_concat")([h, sensors])
    out = tf.keras.layers.Dense(NUM_CLASSES, activation="softmax", name="res_out")(merged)
    return tf.keras.Model(inp, out, name="ResMLP_Tiny")


def make_modality_gated(input_dim=BASE_DIM):
    inp = tf.keras.layers.Input(shape=(input_dim,), name="gate_in")
    sensors = tf.keras.layers.Lambda(lambda x: x[:, :3], name="sensors_only")(inp)
    context = tf.keras.layers.Lambda(lambda x: x[:, 3:], name="context_only")(inp)

    s_feat = tf.keras.layers.Dense(24, activation="relu", name="s_proj")(sensors)
    c_gate = tf.keras.layers.Dense(24, activation="sigmoid", name="c_gate")(context)
    # Quando context = 0, c_gate depende só do bias (~0.5) e não bloqueia s_feat!
    modulated = tf.keras.layers.Multiply(name="modulate")([s_feat, c_gate])
    combined = tf.keras.layers.Add(name="res_sensor")([s_feat, modulated])
    h = tf.keras.layers.Dense(16, activation="relu", name="head_d1")(combined)
    out = tf.keras.layers.Dense(NUM_CLASSES, activation="softmax", name="gate_out")(h)
    return tf.keras.Model(inp, out, name="ModalityGated_TinyNet")


def train_keras_model(model, X_tr, y_tr, X_va, y_va, class_weights, epochs=60, lr=0.003):
    tf.keras.utils.set_random_seed(SEED)
    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=lr),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )
    cbs = [
        tf.keras.callbacks.EarlyStopping(monitor="val_loss", patience=10, restore_best_weights=True, verbose=0),
        tf.keras.callbacks.ReduceLROnPlateau(monitor="val_loss", factor=0.5, patience=4, min_lr=1e-5, verbose=0),
    ]
    hist = model.fit(
        X_tr, y_tr,
        validation_data=(X_va, y_va),
        epochs=epochs,
        batch_size=128,
        class_weight=class_weights,
        callbacks=cbs,
        verbose=0,
    )
    return {k: [float(v) for v in vals] for k, vals in hist.history.items()}


def export_tflite_variants(model, X_rep, name):
    # 1. Float32 puro
    c_f32 = tf.lite.TFLiteConverter.from_keras_model(model)
    b_f32 = c_f32.convert()

    # 2. Notebook style (Optimize.DEFAULT sem representative_dataset)
    c_def = tf.lite.TFLiteConverter.from_keras_model(model)
    c_def.optimizations = [tf.lite.Optimize.DEFAULT]
    b_def = c_def.convert()

    # 3. Float16
    c_f16 = tf.lite.TFLiteConverter.from_keras_model(model)
    c_f16.optimizations = [tf.lite.Optimize.DEFAULT]
    c_f16.target_spec.supported_types = [tf.float16]
    b_f16 = c_f16.convert()

    # 4. Full INT8 com representative_dataset
    def rep_gen():
        for i in range(min(300, len(X_rep))):
            yield [X_rep[i:i + 1].astype(np.float32)]

    c_int8 = tf.lite.TFLiteConverter.from_keras_model(model)
    c_int8.optimizations = [tf.lite.Optimize.DEFAULT]
    c_int8.representative_dataset = rep_gen
    c_int8.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    c_int8.inference_input_type = tf.int8
    c_int8.inference_output_type = tf.int8
    b_int8 = c_int8.convert()

    return {
        "float32_bytes": len(b_f32),
        "notebook_default_bytes": len(b_def),
        "float16_bytes": len(b_f16),
        "int8_bytes": len(b_int8),
        "tflite_int8_blob": b_int8,
        "tflite_f32_blob": b_f32,
    }


def eval_int8_fidelity(keras_model, tflite_blob, X_test):
    interp = tf.lite.Interpreter(model_content=tflite_blob)
    interp.allocate_tensors()
    in_det = interp.get_input_details()[0]
    out_det = interp.get_output_details()[0]
    in_scale, in_zp = in_det["quantization"]
    out_scale, out_zp = out_det["quantization"]

    p_keras = keras_model.predict(X_test, verbose=0)
    p_int8 = np.zeros_like(p_keras)

    for i in range(len(X_test)):
        xq = np.round(X_test[i:i + 1] / in_scale + in_zp).astype(np.int8)
        interp.set_tensor(in_det["index"], xq)
        interp.invoke()
        yq = interp.get_tensor(out_det["index"])[0]
        p_int8[i] = (yq.astype(np.float32) - out_zp) * out_scale

    diff = np.abs(p_keras - p_int8)
    agree = float(np.mean(np.argmax(p_keras, axis=1) == np.argmax(p_int8, axis=1)))
    return {
        "max_abs_diff": float(np.max(diff)),
        "mean_abs_diff": float(np.mean(diff)),
        "decision_agreement": agree,
        "abs_diffs_sample": diff.ravel()[:2000].astype(np.float32),
        "p_keras_sample": p_keras[:500].astype(np.float32),
        "p_int8_sample": p_int8[:500].astype(np.float32),
    }


# ============================================================================
# 5. EXECUÇÃO PRINCIPAL DO EXPERIMENTO
# ============================================================================
def main():
    t0 = time.time()
    print("[1/7] Carregando e inspecionando dataset...")
    df_raw = pd.read_csv(DATA_PATH)
    df_raw["soil_id"] = df_raw["soil_type"].map(SOIL_TO_ID)
    df_raw["crop_id"] = df_raw["crop ID"].map(CROP_TO_ID)
    df_raw["stage_id"] = df_raw["Seedling Stage"].map(STAGE_TO_ID)
    df_raw["strat_key"] = df_raw["crop ID"].astype(str) + "_" + df_raw["result"].astype(str)

    # Split 1: Original Buggy (com duplicatas)
    tr_orig, tmp_orig = stratified_split(df_raw, "strat_key", test_size=0.30, seed=SEED)
    va_orig, te_orig = stratified_split(tmp_orig, "strat_key", test_size=0.50, seed=SEED)

    # Split 2: Clean Deduplicated Stratified Split
    df_clean = df_raw.drop_duplicates().reset_index(drop=True)
    tr_clean, tmp_clean = stratified_split(df_clean, "strat_key", test_size=0.30, seed=SEED)
    va_clean, te_clean = stratified_split(tmp_clean, "strat_key", test_size=0.50, seed=SEED)

    # Split 3: Strict Group Split por Tupla de Sensores (MOI, temp, humidity) - Zero Leakage!
    tr_grp, va_grp, te_grp = group_split_by_sensors(df_clean, seed=SEED)
    print(f"  Raw: {len(df_raw)} | Clean (dedup): {len(df_clean)} | GroupSplit Te: {len(te_grp)}")

    # Conjuntos de treino e validação
    X_tr_orig, y_tr_orig = build_original_buggy_dataset(tr_orig, is_training=True)
    X_va_orig, y_va_orig = build_original_buggy_dataset(va_orig, is_training=True)
    u_orig, c_orig = np.unique(y_tr_orig, return_counts=True)
    w_orig_bal = {int(c): float(len(y_tr_orig) / (NUM_CLASSES * cnt)) for c, cnt in zip(u_orig, c_orig)}

    X_tr_c23, y_tr_c23 = build_clean_full8_dataset(tr_clean, with_physics=False, include_dropouts=True)
    X_va_c23, y_va_c23 = build_clean_full8_dataset(va_clean, with_physics=False, include_dropouts=True)

    X_tr_c27, y_tr_c27 = build_clean_full8_dataset(tr_clean, with_physics=True, include_dropouts=True)
    X_va_c27, y_va_c27 = build_clean_full8_dataset(va_clean, with_physics=True, include_dropouts=True)

    X_tr_g27, y_tr_g27 = build_clean_full8_dataset(tr_grp, with_physics=True, include_dropouts=True)
    X_va_g27, y_va_g27 = build_clean_full8_dataset(va_grp, with_physics=True, include_dropouts=True)

    # Pesos calibrados (evita colapso de Precisão da Classe 2)
    w_calib = {0: 1.0, 1: 1.15, 2: 2.20}

    print("[2/7] Treinando 7 arquiteturas neurais TinyML...")
    neural_specs = [
        ("MLP-Orig (Buggy)", make_mlp(BASE_DIM, [32, 16], "mlp_orig"), X_tr_orig, y_tr_orig, X_va_orig, y_va_orig, w_orig_bal, False, te_orig),
        ("MLP-Clean (32-16)", make_mlp(BASE_DIM, [32, 16], "mlp_clean"), X_tr_c23, y_tr_c23, X_va_c23, y_va_c23, w_calib, False, te_clean),
        ("MLP-Nano (16-8)", make_mlp(BASE_DIM, [16, 8], "mlp_nano"), X_tr_c23, y_tr_c23, X_va_c23, y_va_c23, w_calib, False, te_clean),
        ("MLP-Deep-BN (64-32-16)", make_mlp(BASE_DIM, [64, 32, 16], "mlp_deep", use_bn=True, dropout=0.10), X_tr_c23, y_tr_c23, X_va_c23, y_va_c23, w_calib, False, te_clean),
        ("ResMLP-Tiny", make_res_mlp(BASE_DIM), X_tr_c23, y_tr_c23, X_va_c23, y_va_c23, w_calib, False, te_clean),
        ("PhysMLP-27 (+VPD)", make_mlp(PHYS_DIM, [32, 16], "phys_mlp"), X_tr_c27, y_tr_c27, X_va_c27, y_va_c27, w_calib, True, te_clean),
        ("Modality-Gated", make_modality_gated(BASE_DIM), X_tr_c23, y_tr_c23, X_va_c23, y_va_c23, w_calib, False, te_clean),
    ]

    trained_models = {}
    histories = {}
    results = {}
    quant_results = {}

    for mname, model, Xtr, ytr, Xva, yva, cw, is_phys, te_df in neural_specs:
        print(f"  -> Treinando {mname} (params={model.count_params()})...")
        hist = train_keras_model(model, Xtr, ytr, Xva, yva, cw, epochs=60, lr=0.003)
        histories[mname] = hist
        trained_models[mname] = (model, is_phys, te_df)

        # Avaliação nos 8 modos
        mode_res = {}
        for mode_label, mtuple in ALL_8_MASKS:
            Xm, ym = build_mode_eval(te_df, mtuple, with_physics=is_phys)
            pm = model.predict(Xm, verbose=0)
            yhat = np.argmax(pm, axis=1)
            mode_res[mode_label] = compute_metrics(ym, yhat, pm)

        # Group-split eval para PhysMLP e MLP-Clean
        if mname in ("MLP-Clean (32-16)", "PhysMLP-27 (+VPD)"):
            m_grp = make_mlp(PHYS_DIM if is_phys else BASE_DIM, [32, 16], f"grp_{mname[:4]}")
            Xg_tr, yg_tr = build_clean_full8_dataset(tr_grp, with_physics=is_phys, include_dropouts=True)
            Xg_va, yg_va = build_clean_full8_dataset(va_grp, with_physics=is_phys, include_dropouts=True)
            train_keras_model(m_grp, Xg_tr, yg_tr, Xg_va, yg_va, w_calib, epochs=60, lr=0.003)
            for ml, mt in [("M1: Completo [S,C,St]", (1, 1, 1)), ("M6: Apenas Cultura [0,C,0]", (0, 1, 0)), ("M8: Zero Contexto [0,0,0]", (0, 0, 0))]:
                Xmg, ymg = build_mode_eval(te_grp, mt, with_physics=is_phys)
                pmg = m_grp.predict(Xmg, verbose=0)
                mode_res[f"GRP_{ml}"] = compute_metrics(ymg, np.argmax(pmg, axis=1), pmg)

        # Quantização TFLite + INT8 fidelity
        qinfo = export_tflite_variants(model, Xtr, mname)
        X_eval_q, _ = build_mode_eval(te_df, (1, 1, 1), with_physics=is_phys)
        fid = eval_int8_fidelity(model, qinfo["tflite_int8_blob"], X_eval_q[:600])
        quant_results[mname] = {
            "params": int(model.count_params()),
            "float32_bytes": qinfo["float32_bytes"],
            "notebook_default_bytes": qinfo["notebook_default_bytes"],
            "float16_bytes": qinfo["float16_bytes"],
            "int8_bytes": qinfo["int8_bytes"],
            "max_abs_diff": fid["max_abs_diff"],
            "mean_abs_diff": fid["mean_abs_diff"],
            "decision_agreement": fid["decision_agreement"],
        }
        if mname == "PhysMLP-27 (+VPD)":
            np.savez(
                os.path.join(OUT_DIR, "quant_fidelity_detail.npz"),
                abs_diffs=fid["abs_diffs_sample"],
                p_keras=fid["p_keras_sample"],
                p_int8=fid["p_int8_sample"],
            )
        results[mname] = mode_res

    print("[3/7] Treinando 3 modelos baseados em Árvores (CART, Random Forest, GBDT)...")
    # Sample weights alinhados com w_calib
    sw_c27 = np.array([w_calib[int(y)] for y in y_tr_c27], dtype=np.float64)

    cart_model = FastCARTClassifier(max_depth=9, min_samples_leaf=8, seed=SEED).fit(X_tr_c27, y_tr_c27, sample_weight=sw_c27)
    rf_model = FastRandomForest(n_trees=15, max_depth=8, min_samples_leaf=8, seed=SEED).fit(X_tr_c27, y_tr_c27, sample_weight=sw_c27)
    gbdt_model = FastGBDTMulticlass(n_estimators=35, lr=0.18, max_depth=4, min_samples_leaf=10, seed=SEED).fit(X_tr_c27, y_tr_c27)

    # Group split GBDT
    gbdt_grp = FastGBDTMulticlass(n_estimators=35, lr=0.18, max_depth=4, min_samples_leaf=10, seed=SEED).fit(X_tr_g27, y_tr_g27)

    tree_specs = [
        ("CART-Tree (Depth 9)", cart_model, cart_model.n_nodes),
        ("RandomForest (15x8)", rf_model, rf_model.total_nodes()),
        ("GBDT-Tabular (35x4)", gbdt_model, gbdt_model.total_nodes()),
    ]
    for tname, tmod, n_nodes in tree_specs:
        mode_res = {}
        for mode_label, mtuple in ALL_8_MASKS:
            Xm, ym = build_mode_eval(te_clean, mtuple, with_physics=True)
            pm = tmod.predict_proba(Xm)
            mode_res[mode_label] = compute_metrics(ym, np.argmax(pm, axis=1), pm)
        if tname == "GBDT-Tabular (35x4)":
            for ml, mt in [("M1: Completo [S,C,St]", (1, 1, 1)), ("M6: Apenas Cultura [0,C,0]", (0, 1, 0)), ("M8: Zero Contexto [0,0,0]", (0, 0, 0))]:
                Xmg, ymg = build_mode_eval(te_grp, mt, with_physics=True)
                pmg = gbdt_grp.predict_proba(Xmg)
                mode_res[f"GRP_{ml}"] = compute_metrics(ymg, np.argmax(pmg, axis=1), pmg)
        results[tname] = mode_res
        quant_results[tname] = {
            "params": int(n_nodes),
            "float32_bytes": int(n_nodes * 16),
            "int8_bytes": int(n_nodes * 8),
            "tensor_arena_bytes": 0 if "CART" in tname or "Random" in tname else 256,
        }

    print("[4/7] Executando varredura de Class Weight (Pareto Classe 2)...")
    w2_sweep = [1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.05]
    sweep_results = []
    for w2 in w2_sweep:
        m_sw = make_mlp(BASE_DIM, [32, 16], f"sw_{w2}")
        train_keras_model(m_sw, X_tr_c23, y_tr_c23, X_va_c23, y_va_c23, {0: 1.0, 1: 1.1, 2: w2}, epochs=40, lr=0.003)
        Xm4, ym4 = build_mode_eval(te_clean, (0, 0, 0), with_physics=False)
        pm4 = m_sw.predict(Xm4, verbose=0)
        met4 = compute_metrics(ym4, np.argmax(pm4, axis=1), pm4)
        Xm1, ym1 = build_mode_eval(te_clean, (1, 1, 1), with_physics=False)
        pm1 = m_sw.predict(Xm1, verbose=0)
        met1 = compute_metrics(ym1, np.argmax(pm1, axis=1), pm1)
        sweep_results.append({
            "w2": w2,
            "m8_acc": met4["accuracy"], "m8_c2_prec": met4["precision"][2],
            "m8_c2_rec": met4["recall"][2], "m8_c2_f1": met4["f1"][2],
            "m1_acc": met1["accuracy"], "m1_c2_f1": met1["f1"][2],
        })

    print("[5/7] Gerando curvas ROC/PR, Permutation Importance e Limites de Decisão 2D...")
    # ROC/PR e Threshold tuning para MLP-Orig, MLP-Clean, PhysMLP-27, GBDT no Modo 8 (Zero Contexto) e Modo 1
    roc_pr_data = {}
    for name, mod, is_phys, te_df in [
        ("MLP-Orig (Buggy)", trained_models["MLP-Orig (Buggy)"][0], False, te_orig),
        ("MLP-Clean (32-16)", trained_models["MLP-Clean (32-16)"][0], False, te_clean),
        ("PhysMLP-27 (+VPD)", trained_models["PhysMLP-27 (+VPD)"][0], True, te_clean),
    ]:
        Xm8, ym8 = build_mode_eval(te_df, (0, 0, 0), with_physics=is_phys)
        pm8 = mod.predict(Xm8, verbose=0)
        roc_pr_data[name] = {}
        for c in range(NUM_CLASSES):
            fpr, tpr, r_auc, rec, prec, p_auc = compute_roc_pr_curves(ym8, pm8, c)
            roc_pr_data[name][f"c{c}_fpr"] = fpr.astype(np.float32)
            roc_pr_data[name][f"c{c}_tpr"] = tpr.astype(np.float32)
            roc_pr_data[name][f"c{c}_roc_auc"] = r_auc
            roc_pr_data[name][f"c{c}_rec"] = rec.astype(np.float32)
            roc_pr_data[name][f"c{c}_prec"] = prec.astype(np.float32)
            roc_pr_data[name][f"c{c}_auprc"] = p_auc
        roc_pr_data[name]["ym8"] = ym8
        roc_pr_data[name]["pm8"] = pm8

    # GBDT ROC/PR
    Xm8_g, ym8_g = build_mode_eval(te_clean, (0, 0, 0), with_physics=True)
    pm8_g = gbdt_model.predict_proba(Xm8_g)
    roc_pr_data["GBDT-Tabular (35x4)"] = {"ym8": ym8_g, "pm8": pm8_g}
    for c in range(NUM_CLASSES):
        fpr, tpr, r_auc, rec, prec, p_auc = compute_roc_pr_curves(ym8_g, pm8_g, c)
        roc_pr_data["GBDT-Tabular (35x4)"][f"c{c}_fpr"] = fpr.astype(np.float32)
        roc_pr_data["GBDT-Tabular (35x4)"][f"c{c}_tpr"] = tpr.astype(np.float32)
        roc_pr_data["GBDT-Tabular (35x4)"][f"c{c}_roc_auc"] = r_auc
        roc_pr_data["GBDT-Tabular (35x4)"][f"c{c}_rec"] = rec.astype(np.float32)
        roc_pr_data["GBDT-Tabular (35x4)"][f"c{c}_prec"] = prec.astype(np.float32)
        roc_pr_data["GBDT-Tabular (35x4)"][f"c{c}_auprc"] = p_auc

    # Permutation Importance em PhysMLP-27 (Modo 1 Completo)
    X_pi, y_pi = build_mode_eval(te_clean, (1, 1, 1), with_physics=True)
    base_acc = float(np.mean(np.argmax(trained_models["PhysMLP-27 (+VPD)"][0].predict(X_pi, verbose=0), axis=1) == y_pi))
    feat_names = (
        ["temp", "humidity", "MOI"]
        + [f"Soil:{s}" for s in SOIL_TYPES]
        + [f"Crop:{c}" for c in CROP_TYPES]
        + [f"Stage:{st[:12]}" for st in SEEDLING_STAGES]
        + ["VPD_kPa", "EvapDemand", "Temp_MOI_Ratio", "MOI_x_Temp"]
    )
    perm_imp = []
    rng_pi = np.random.default_rng(SEED)
    for j in range(X_pi.shape[1]):
        Xp = X_pi.copy()
        rng_pi.shuffle(Xp[:, j])
        acc_j = float(np.mean(np.argmax(trained_models["PhysMLP-27 (+VPD)"][0].predict(Xp, verbose=0), axis=1) == y_pi))
        perm_imp.append(base_acc - acc_j)

    # 2D Decision Boundaries (MOI x temp em RH = 35, 60, 85)
    moi_grid = np.linspace(0, 100, 80)
    temp_grid = np.linspace(13, 46, 80)
    MG, TG = np.meshgrid(moi_grid, temp_grid)
    mg_flat, tg_flat = MG.ravel().astype(np.float32), TG.ravel().astype(np.float32)
    boundary_grids = {}
    for rh in [35.0, 60.0, 85.0]:
        hg_flat = np.full_like(mg_flat, rh)
        zeros_i = np.zeros(len(mg_flat), dtype=np.int32)
        X_g23 = encode_batch(tg_flat, hg_flat, mg_flat, zeros_i, zeros_i, zeros_i, with_physics=False)
        X_g27 = encode_batch(tg_flat, hg_flat, mg_flat, zeros_i, zeros_i, zeros_i, with_physics=True)
        boundary_grids[f"orig_rh{int(rh)}"] = np.argmax(trained_models["MLP-Orig (Buggy)"][0].predict(X_g23, verbose=0), axis=1).reshape(MG.shape)
        boundary_grids[f"clean_rh{int(rh)}"] = np.argmax(trained_models["MLP-Clean (32-16)"][0].predict(X_g23, verbose=0), axis=1).reshape(MG.shape)
        boundary_grids[f"phys_rh{int(rh)}"] = np.argmax(trained_models["PhysMLP-27 (+VPD)"][0].predict(X_g27, verbose=0), axis=1).reshape(MG.shape)
        boundary_grids[f"gbdt_rh{int(rh)}"] = gbdt_model.predict(X_g27).reshape(MG.shape)

    print("[6/7] Simulando 48 horas no ESP32 (Paulista-PE + Falha DHT22 -128/255)...")
    hours = np.arange(0, 48, 0.5)
    sim_temp = 27.5 + 8.5 * np.sin(2 * np.pi * (hours - 9) / 24.0)
    sim_hum = 65.0 - 22.0 * np.sin(2 * np.pi * (hours - 9) / 24.0)
    sim_moi = np.clip(52.0 - 0.85 * hours + np.where(hours >= 26, 38.0, 0.0), 12.0, 88.0)
    # Injetar falha de sensor DHT22 entre t=34h e t=36h (temp=-128, hum=255 como em SensorsManager.cpp)
    sim_temp_raw = sim_temp.copy()
    sim_hum_raw = sim_hum.copy()
    fault_mask = (hours >= 34.0) & (hours <= 36.0)
    sim_temp_raw[fault_mask] = -128.0
    sim_hum_raw[fault_mask] = 255.0

    zeros_sim = np.zeros(len(hours), dtype=np.int32)
    crops_tom = np.full(len(hours), CROP_TO_ID["Tomato"], dtype=np.int32)
    # Sem proteção de sentinela (como o notebook exporta atualmente)
    X_sim_orig = encode_batch(sim_temp_raw.astype(np.float32), sim_hum_raw.astype(np.float32), sim_moi.astype(np.float32), zeros_sim, zeros_sim, zeros_sim, False)
    p_sim_orig = trained_models["MLP-Orig (Buggy)"][0].predict(X_sim_orig, verbose=0)

    X_sim_phys = encode_batch(sim_temp_raw.astype(np.float32), sim_hum_raw.astype(np.float32), sim_moi.astype(np.float32), zeros_sim, crops_tom, zeros_sim, True)
    p_sim_phys = trained_models["PhysMLP-27 (+VPD)"][0].predict(X_sim_phys, verbose=0)
    # Com proteção de sentinela no firmware, fault_mask força decisão segura OFF/ERROR
    p_sim_phys_safe = p_sim_phys.copy()
    p_sim_phys_safe[fault_mask] = [1.0, 0.0, 0.0]

    print("[7/7] Salvando resultados completos em output/...")
    summary = {
        "dataset_stats": {
            "total_rows": int(len(df_raw)),
            "exact_duplicates": int(df_raw.duplicated().sum()),
            "clean_rows": int(len(df_clean)),
            "unique_sensor_tuples": int(df_clean.groupby(["MOI", "temp", "humidity"]).ngroups),
        },
        "results": results,
        "quant_results": quant_results,
        "histories": histories,
        "sweep_results": sweep_results,
        "feat_names": feat_names,
        "perm_imp": [float(x) for x in perm_imp],
        "roc_auc": {
            m: {f"c{c}": roc_pr_data[m][f"c{c}_roc_auc"] for c in range(NUM_CLASSES)}
            for m in roc_pr_data
        },
        "auprc": {
            m: {f"c{c}": roc_pr_data[m][f"c{c}_auprc"] for c in range(NUM_CLASSES)}
            for m in roc_pr_data
        },
    }
    with open(os.path.join(OUT_DIR, "benchmark_summary.json"), "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)

    np.savez(
        os.path.join(OUT_DIR, "benchmark_arrays.npz"),
        moi_grid=moi_grid, temp_grid=temp_grid,
        **boundary_grids,
        hours=hours, sim_temp=sim_temp_raw, sim_hum=sim_hum_raw, sim_moi=sim_moi,
        fault_mask=fault_mask, p_sim_orig=p_sim_orig, p_sim_phys_raw=p_sim_phys, p_sim_phys_safe=p_sim_phys_safe,
        **{f"{m}_{k}": v for m, d in roc_pr_data.items() for k, v in d.items() if isinstance(v, np.ndarray)},
    )
    print(f"Concluído em {time.time() - t0:.1f}s!")


if __name__ == "__main__":
    main()
