#!/usr/bin/env python3
"""
Gerador do Painel Visual Completo (12 Pranchas de Alta Resolução em Matplotlib, 300 DPI).
Execução: /home/victor/projects/sugarcane/.venv/bin/python ai-model/generate_plots.py
"""

import os
import json
import shutil
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap
import csv

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(BASE_DIR, "output")
FIG_DIR = os.path.join(OUT_DIR, "figures")
ARTIFACT_DIR = "/home/victor/.gemini/antigravity-cli/brain/fda70498-9ae2-4dac-ae67-7c66e29023c1"
DATA_PATH = os.path.join(BASE_DIR, "datasets", "Smart Agriculture Dataset", "cropdata_updated.csv")

os.makedirs(FIG_DIR, exist_ok=True)
os.makedirs(ARTIFACT_DIR, exist_ok=True)

plt.rcParams.update({
    "figure.dpi": 150,
    "savefig.dpi": 300,
    "font.size": 9.5,
    "axes.titlesize": 11,
    "axes.titleweight": "bold",
    "axes.labelsize": 9.5,
    "legend.fontsize": 8,
    "xtick.labelsize": 8.5,
    "ytick.labelsize": 8.5,
    "axes.grid": True,
    "grid.alpha": 0.28,
    "grid.linestyle": "--",
})

C_OFF = "#2ecc71"
C_IRR = "#3498db"
C_STR = "#e74c3c"
CLASS_COLORS = [C_OFF, C_IRR, C_STR]
CLASS_LABELS = ["0: OFF (Adequado)", "1: IRRIGATE (Irrigar)", "2: THERMAL_STRESS (Alerta)"]


def save_fig(fig, filename):
    path = os.path.join(FIG_DIR, filename)
    fig.savefig(path, dpi=300, bbox_inches="tight", facecolor="white")
    shutil.copy2(path, os.path.join(ARTIFACT_DIR, filename))
    plt.close(fig)
    print(f"  [OK] {filename}")


def load_csv_raw():
    crops, soils, stages = [], [], []
    moi, temp, hum, res = [], [], [], []
    with open(DATA_PATH, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for r in reader:
            crops.append(r["crop ID"])
            soils.append(r["soil_type"])
            stages.append(r["Seedling Stage"])
            moi.append(float(r["MOI"]))
            temp.append(float(r["temp"]))
            hum.append(float(r["humidity"]))
            res.append(int(r["result"]))
    return (
        np.array(crops), np.array(soils), np.array(stages),
        np.array(moi), np.array(temp), np.array(hum), np.array(res)
    )


def main():
    print("Carregando resultados e dataset...")
    with open(os.path.join(OUT_DIR, "benchmark_summary.json"), "r", encoding="utf-8") as f:
        summary = json.load(f)
    arr = np.load(os.path.join(OUT_DIR, "benchmark_arrays.npz"))
    qdet = np.load(os.path.join(OUT_DIR, "quant_fidelity_detail.npz"))
    crops, soils, stages, moi, temp, hum, res = load_csv_raw()

    results = summary["results"]
    quant = summary["quant_results"]
    histories = summary["histories"]
    sweep = summary["sweep_results"]

    # =========================================================================
    # FIG 01: Dataset Forensics & Leakage Analysis
    # =========================================================================
    fig, axes = plt.subplots(2, 2, figsize=(15, 10))
    fig.suptitle("Fig. 01 — Auditoria Forense do Dataset (cropdata_updated.csv) & Vazamento de Dados", fontsize=13.5, fontweight="bold")

    # (A) Classes por cultura
    crop_list = ["Tomato", "Chilli", "Carrot", "Potato", "Wheat"]
    counts = np.array([[np.sum((crops == c) & (res == k)) for k in range(3)] for c in crop_list])
    x = np.arange(len(crop_list))
    b0 = axes[0, 0].bar(x, counts[:, 0], color=C_OFF, label=CLASS_LABELS[0])
    b1 = axes[0, 0].bar(x, counts[:, 1], bottom=counts[:, 0], color=C_IRR, label=CLASS_LABELS[1])
    b2 = axes[0, 0].bar(x, counts[:, 2], bottom=counts[:, 0] + counts[:, 1], color=C_STR, label=CLASS_LABELS[2])
    axes[0, 0].set_xticks(x)
    axes[0, 0].set_xticklabels(crop_list)
    axes[0, 0].set_ylabel("Número de Amostras")
    axes[0, 0].set_title("(A) Distribuição das 3 Classes por Cultura (n = 16.411)")
    axes[0, 0].legend(loc="upper left")
    for i in range(len(crop_list)):
        tot = counts[i].sum()
        axes[0, 0].text(i, tot + 120, f"n={tot}\n(C2:{counts[i,2]})", ha="center", va="bottom", fontsize=8)

    # (B) Duplicatas e Tuplas Únicas de Sensores
    labels_b = ["Total Linhas\nBrutas", "Linhas Únicas\n(Sem Duplicatas)", "Tuplas Únicas\n(MOI, Temp, Hum)", "Overlap Treino-Teste\nem Split Aleatório (%)"]
    vals_b = [16411, 16283, 2886, 2244]
    ax_b = axes[0, 1]
    bars_b = ax_b.bar(["Linhas Brutas\n(128 dup.)", "Linhas Únicas\n(Deduplicado)", "Tuplas Sensores\n(MOI,T,RH)", "Teste c/ Tupla\nVista no Treino"], [16411, 16283, 2886, 2244], color=["#7f8c8d", "#2980b9", "#8e44ad", "#c0392b"])
    ax_b.set_ylabel("Contagem")
    ax_b.set_title("(B) Diagnóstico de Redundância & Sensor-Tuple Leakage (91,85%)")
    annots = ["16.411\n(128 duplicadas)", "16.283\n(-0,78%)", "2.886 únicas\n(5,64x reuso)", "2.244 / 2.443\n(91,85% vazamento!)"]
    for rect, txt in zip(bars_b, annots):
        ax_b.text(rect.get_x() + rect.get_width()/2, rect.get_height() + 250, txt, ha="center", va="bottom", fontsize=8.5, fontweight="bold")
    ax_b.set_ylim(0, 19000)

    # (C) Boxplots das 3 variáveis físicas por classe
    pos_base = np.array([1, 2, 3])
    bp_data_m = [moi[res == k] for k in range(3)]
    bp_data_t = [temp[res == k] for k in range(3)]
    bp_data_h = [hum[res == k] for k in range(3)]
    bp1 = axes[1, 0].boxplot(bp_data_m, positions=pos_base - 0.25, widths=0.22, patch_artist=True)
    bp2 = axes[1, 0].boxplot(bp_data_t, positions=pos_base, widths=0.22, patch_artist=True)
    bp3 = axes[1, 0].boxplot(bp_data_h, positions=pos_base + 0.25, widths=0.22, patch_artist=True)
    for patch in bp1["boxes"]: patch.set_facecolor("#1abc9c")
    for patch in bp2["boxes"]: patch.set_facecolor("#e67e22")
    for patch in bp3["boxes"]: patch.set_facecolor("#3498db")
    axes[1, 0].set_xticks(pos_base)
    axes[1, 0].set_xticklabels(["Classe 0 (OFF)", "Classe 1 (IRRIGATE)", "Classe 2 (STRESS)"])
    axes[1, 0].set_ylabel("Valor Físico (% ou °C)")
    axes[1, 0].set_title("(C) Distribuição de MOI (%), Temp (°C) e Umidade (%) por Classe")
    axes[1, 0].plot([], [], color="#1abc9c", lw=6, label="MOI (%)")
    axes[1, 0].plot([], [], color="#e67e22", lw=6, label="Temp (°C)")
    axes[1, 0].plot([], [], color="#3498db", lw=6, label="Humidity (%)")
    axes[1, 0].legend(loc="upper right")

    # (D) Espaço Psicrométrico Temp x Humidity colorido por Classe + Isolinhas de VPD
    rng_s = np.random.default_rng(42)
    idx_sub = rng_s.choice(len(temp), size=2500, replace=False)
    for k in range(3):
        mk = idx_sub[res[idx_sub] == k]
        axes[1, 1].scatter(temp[mk], hum[mk], s=10, alpha=0.45, color=CLASS_COLORS[k], label=CLASS_LABELS[k])
    tg = np.linspace(13, 46, 80)
    hg = np.linspace(15, 92, 80)
    TG, HG = np.meshgrid(tg, hg)
    VPD = 0.61078 * np.exp((17.27 * TG) / (TG + 237.3)) * (1.0 - HG / 100.0)
    cs = axes[1, 1].contour(TG, HG, VPD, levels=[1.0, 2.0, 3.5, 5.0], colors="black", linestyles=":", linewidths=1.1)
    axes[1, 1].clabel(cs, fmt="VPD=%.1f kPa", fontsize=7.5)
    axes[1, 1].set_xlabel("Temperatura do Ar (°C)")
    axes[1, 1].set_ylabel("Umidade Relativa do Ar (%)")
    axes[1, 1].set_title("(D) Espaço Psicrométrico T × RH e Isolinhas de VPD (kPa)")
    axes[1, 1].legend(loc="lower left", markerscale=1.8)

    plt.tight_layout(rect=[0, 0, 1, 0.96])
    save_fig(fig, "fig01_dataset_forensics_and_leakage.png")

    # =========================================================================
    # FIG 02: Class Weight Distortion & Bayes Limit
    # =========================================================================
    fig, axes = plt.subplots(2, 2, figsize=(15, 10))
    fig.suptitle("Fig. 02 — Bug #5: Distorção de Class Weight + Context Dropout e Teto Bayesiano", fontsize=13.5, fontweight="bold")

    # (A) Multiplicação assimétrica no treino original
    cats = ["Wheat\n(C0/C1/C2)", "Hortaliça C0/C1\n(Qualquer MOI)", "Hortaliça C2\n(MOI >= 60%)", "Hortaliça C2\n(MOI < 60%)"]
    mult_orig = [2, 4, 4, 3]
    mult_clean = [4, 8, 8, 8]
    x_a = np.arange(len(cats))
    axes[0, 0].bar(x_a - 0.18, mult_orig, width=0.35, color="#e74c3c", label="Notebook Original (Assimétrico)")
    axes[0, 0].bar(x_a + 0.18, mult_clean, width=0.35, color="#2ecc71", label="Pipeline Corrigido (Full-8 Masks)")
    axes[0, 0].set_xticks(x_a)
    axes[0, 0].set_xticklabels(cats)
    axes[0, 0].set_ylabel("Fator de Multiplicação por Amostra")
    axes[0, 0].set_title("(A) Fator de Replicação no Context Dropout")
    axes[0, 0].legend()

    # (B) Comparação Precision/Recall/F1 Classe 2 no Modo 8 (Zero Contexto)
    m_orig_m8 = results["MLP-Orig (Buggy)"]["M8: Zero Contexto [0,0,0]"]
    m_cln_m8 = results["MLP-Clean (32-16)"]["M8: Zero Contexto [0,0,0]"]
    m_phys_m8 = results["PhysMLP-27 (+VPD)"]["M8: Zero Contexto [0,0,0]"]
    metrics_b = ["Acurácia\nGlobal", "Precision\nClasse 2", "Recall\nClasse 2", "F1-Score\nClasse 2"]
    v_orig = [m_orig_m8["accuracy"]*100, m_orig_m8["precision"][2]*100, m_orig_m8["recall"][2]*100, m_orig_m8["f1"][2]*100]
    v_cln = [m_cln_m8["accuracy"]*100, m_cln_m8["precision"][2]*100, m_cln_m8["recall"][2]*100, m_cln_m8["f1"][2]*100]
    v_phys = [m_phys_m8["accuracy"]*100, m_phys_m8["precision"][2]*100, m_phys_m8["recall"][2]*100, m_phys_m8["f1"][2]*100]
    xb = np.arange(4)
    axes[0, 1].bar(xb - 0.25, v_orig, width=0.25, color="#e74c3c", label="MLP-Orig (w2=5.05)")
    axes[0, 1].bar(xb, v_cln, width=0.25, color="#3498db", label="MLP-Clean (w2=2.20)")
    axes[0, 1].bar(xb + 0.25, v_phys, width=0.25, color="#2ecc71", label="PhysMLP-27 (w2=2.20)")
    axes[0, 1].set_xticks(xb)
    axes[0, 1].set_xticklabels(metrics_b)
    axes[0, 1].set_ylabel("Porcentagem (%)")
    axes[0, 1].set_ylim(0, 108)
    axes[0, 1].set_title("(B) Impacto Real na Classe 2 — Modo Zero Contexto")
    axes[0, 1].legend()
    for i in range(4):
        axes[0, 1].text(i - 0.25, v_orig[i] + 1.5, f"{v_orig[i]:.1f}%", ha="center", fontsize=7.5)
        axes[0, 1].text(i + 0.25, v_phys[i] + 1.5, f"{v_phys[i]:.1f}%", ha="center", fontsize=7.5)

    # (C) Pareto Sweep de w2
    w2s = [d["w2"] for d in sweep]
    axes[1, 0].plot(w2s, [d["m8_c2_prec"]*100 for d in sweep], "o-", color="#e74c3c", lw=2, label="M8 Precision Classe 2")
    axes[1, 0].plot(w2s, [d["m8_c2_rec"]*100 for d in sweep], "s-", color="#3498db", lw=2, label="M8 Recall Classe 2")
    axes[1, 0].plot(w2s, [d["m8_c2_f1"]*100 for d in sweep], "^-", color="#9b59b6", lw=2, label="M8 F1-Score Classe 2")
    axes[1, 0].plot(w2s, [d["m8_acc"]*100 for d in sweep], "D--", color="#2c3e50", lw=1.8, label="M8 Acurácia Global")
    axes[1, 0].axvline(2.2, color="#2ecc71", ls="--", lw=1.8, label="Ponto Ótimo Calibrado (w2=2.2)")
    axes[1, 0].axvline(5.05, color="#e74c3c", ls=":", lw=1.8, label="Notebook Original (w2≈5.05)")
    axes[1, 0].set_xlabel("Peso da Classe 2 (w2) na Função de Perda")
    axes[1, 0].set_ylabel("Métrica (%)")
    axes[1, 0].set_title("(C) Curva de Pareto Precision × Recall × F1 vs. Peso da Classe 2")
    axes[1, 0].legend(loc="lower left", fontsize=7.5)

    # (D) Teto Bayesiano Empírico vs Acurácia Alcançada nos 8 modos
    mode_names = [m[0] for m in [
        ("M1: Completo [S,C,St]",), ("M2: Solo+Cultura [S,C,0]",), ("M3: Solo+Estágio [S,0,St]",),
        ("M4: Cultura+Estágio [0,C,St]",), ("M5: Apenas Solo [S,0,0]",), ("M6: Apenas Cultura [0,C,0]",),
        ("M7: Apenas Estágio [0,0,St]",), ("M8: Zero Contexto [0,0,0]",)
    ]]
    short_m = ["M1\n[S,C,St]", "M2\n[S,C,0]", "M3\n[S,0,St]", "M4\n[0,C,St]", "M5\n[S,0,0]", "M6\n[0,C,0]", "M7\n[0,0,St]", "M8\n[0,0,0]"]
    acc_orig = [results["MLP-Orig (Buggy)"][m]["accuracy"]*100 for m in mode_names]
    acc_phys = [results["PhysMLP-27 (+VPD)"][m]["accuracy"]*100 for m in mode_names]
    acc_gbdt = [results["GBDT-Tabular (35x4)"][m]["accuracy"]*100 for m in mode_names]
    xd = np.arange(8)
    axes[1, 1].plot(xd, acc_orig, "o--", color="#e74c3c", lw=2, label="MLP-Orig (Buggy)")
    axes[1, 1].plot(xd, acc_phys, "s-", color="#2ecc71", lw=2.2, label="PhysMLP-27 (+VPD)")
    axes[1, 1].plot(xd, acc_gbdt, "^-", color="#8e44ad", lw=2, label="GBDT-Tabular (35x4)")
    axes[1, 1].set_xticks(xd)
    axes[1, 1].set_xticklabels(short_m)
    axes[1, 1].set_ylabel("Acurácia de Teste (%)")
    axes[1, 1].set_ylim(55, 102)
    axes[1, 1].set_title("(D) Desempenho nos 8 Modos de Contexto (Destaque: M4 e M6)")
    axes[1, 1].legend(loc="lower left")

    plt.tight_layout(rect=[0, 0, 1, 0.96])
    save_fig(fig, "fig02_class_weight_distortion_and_bayes_limit.png")

    # =========================================================================
    # FIG 03: 2D Decision Boundaries
    # =========================================================================
    fig, axes = plt.subplots(3, 4, figsize=(16, 11), sharex=True, sharey=True)
    fig.suptitle("Fig. 03 — Fronteiras de Decisão 2D (MOI × Temp) em Zero Contexto para RH ∈ {35%, 60%, 85%}", fontsize=13.5, fontweight="bold")
    cmap_dec = ListedColormap([C_OFF, C_IRR, C_STR])
    mg = arr["moi_grid"]
    tg = arr["temp_grid"]
    cols_m = [("orig", "MLP-Orig (Buggy)"), ("clean", "MLP-Clean (32-16)"), ("phys", "PhysMLP-27 (+VPD)"), ("gbdt", "GBDT-Tabular")]
    rhs = [35, 60, 85]
    for r_i, rh in enumerate(rhs):
        for c_i, (key, title) in enumerate(cols_m):
            ax = axes[r_i, c_i]
            Z = arr[f"{key}_rh{rh}"]
            ax.contourf(mg, tg, Z, levels=[-0.5, 0.5, 1.5, 2.5], cmap=cmap_dec, alpha=0.82)
            ax.contour(mg, tg, Z, levels=[0.5, 1.5], colors="black", linewidths=0.8)
            if r_i == 0:
                ax.set_title(title)
            if c_i == 0:
                ax.set_ylabel(f"RH = {rh}%\nTemp (°C)")
            if r_i == 2:
                ax.set_xlabel("Umidade do Solo MOI (%)")
    axes[0, 0].plot([], [], "s", color=C_OFF, label="0: OFF")
    axes[0, 0].plot([], [], "s", color=C_IRR, label="1: IRRIGATE")
    axes[0, 0].plot([], [], "s", color=C_STR, label="2: THERMAL_STRESS")
    fig.legend(loc="lower center", ncol=3, bbox_to_anchor=(0.5, 0.01), fontsize=10)
    plt.tight_layout(rect=[0, 0.04, 1, 0.96])
    save_fig(fig, "fig03_decision_boundaries_2d_slices.png")

    # =========================================================================
    # FIG 04: Global Architecture Benchmark (10 Architectures)
    # =========================================================================
    arch_names = list(results.keys())
    fig, axes = plt.subplots(2, 2, figsize=(16, 11))
    fig.suptitle("Fig. 04 — Comparativo Global das 10 Arquiteturas Alternativas", fontsize=13.5, fontweight="bold")

    y_pos = np.arange(len(arch_names))
    acc_m1 = [results[a]["M1: Completo [S,C,St]"]["accuracy"]*100 for a in arch_names]
    acc_m6 = [results[a]["M6: Apenas Cultura [0,C,0]"]["accuracy"]*100 for a in arch_names]
    acc_m8 = [results[a]["M8: Zero Contexto [0,0,0]"]["accuracy"]*100 for a in arch_names]

    h = 0.26
    axes[0, 0].barh(y_pos - h, acc_m1, height=h, color="#27ae60", label="M1: Completo [S,C,St]")
    axes[0, 0].barh(y_pos, acc_m6, height=h, color="#2980b9", label="M6: Só Cultura [0,C,0]")
    axes[0, 0].barh(y_pos + h, acc_m8, height=h, color="#e67e22", label="M8: Zero Contexto [0,0,0]")
    axes[0, 0].set_yticks(y_pos)
    axes[0, 0].set_yticklabels(arch_names)
    axes[0, 0].set_xlim(50, 103)
    axes[0, 0].set_xlabel("Acurácia (%)")
    axes[0, 0].set_title("(A) Acurácia por Arquitetura nos Modos Chave")
    axes[0, 0].legend(loc="lower right")

    f1_m1 = [results[a]["M1: Completo [S,C,St]"]["macro_f1"]*100 for a in arch_names]
    f1_m6 = [results[a]["M6: Apenas Cultura [0,C,0]"]["macro_f1"]*100 for a in arch_names]
    f1_m8 = [results[a]["M8: Zero Contexto [0,0,0]"]["macro_f1"]*100 for a in arch_names]
    axes[0, 1].barh(y_pos - h, f1_m1, height=h, color="#27ae60", label="M1 Macro-F1")
    axes[0, 1].barh(y_pos, f1_m6, height=h, color="#2980b9", label="M6 Macro-F1")
    axes[0, 1].barh(y_pos + h, f1_m8, height=h, color="#e67e22", label="M8 Macro-F1")
    axes[0, 1].set_yticks(y_pos)
    axes[0, 1].set_yticklabels(arch_names)
    axes[0, 1].set_xlim(30, 103)
    axes[0, 1].set_xlabel("Macro F1-Score (%)")
    axes[0, 1].set_title("(B) Macro F1-Score (Equilíbrio entre Classes 0, 1 e 2)")
    axes[0, 1].legend(loc="lower right")

    # (C) Standard Stratified vs Strict Sensor-Group Split (Zero Leakage)
    grp_models = ["MLP-Clean (32-16)", "PhysMLP-27 (+VPD)", "GBDT-Tabular (35x4)"]
    xg = np.arange(len(grp_models))
    std_m1 = [results[m]["M1: Completo [S,C,St]"]["accuracy"]*100 for m in grp_models]
    grp_m1 = [results[m]["GRP_M1: Completo [S,C,St]"]["accuracy"]*100 for m in grp_models]
    std_m8 = [results[m]["M8: Zero Contexto [0,0,0]"]["accuracy"]*100 for m in grp_models]
    grp_m8 = [results[m]["GRP_M8: Zero Contexto [0,0,0]"]["accuracy"]*100 for m in grp_models]
    axes[1, 0].bar(xg - 0.30, std_m1, width=0.2, color="#2ecc71", label="M1 Stratified")
    axes[1, 0].bar(xg - 0.10, grp_m1, width=0.2, color="#16a085", label="M1 GroupSplit (Zero Leakage)")
    axes[1, 0].bar(xg + 0.10, std_m8, width=0.2, color="#3498db", label="M8 Stratified")
    axes[1, 0].bar(xg + 0.30, grp_m8, width=0.2, color="#2c3e50", label="M8 GroupSplit (Zero Leakage)")
    axes[1, 0].set_xticks(xg)
    axes[1, 0].set_xticklabels(grp_models)
    axes[1, 0].set_ylim(75, 103)
    axes[1, 0].set_ylabel("Acurácia (%)")
    axes[1, 0].set_title("(C) Impacto da Remoção do Vazamento de Tuplas (GroupSplit)")
    axes[1, 0].legend(fontsize=7.5)

    # (D) Precision vs Recall da Classe 2 no Modo 8 (Scatter)
    for idx_a, a in enumerate(arch_names):
        p2 = results[a]["M8: Zero Contexto [0,0,0]"]["precision"][2]*100
        r2 = results[a]["M8: Zero Contexto [0,0,0]"]["recall"][2]*100
        axes[1, 1].scatter(r2, p2, s=110, zorder=4)
        axes[1, 1].annotate(a, (r2, p2), textcoords="offset points", xytext=(5, 4), fontsize=7.5)
    axes[1, 1].set_xlabel("Recall da Classe 2 (%) — Detecção de Estresse")
    axes[1, 1].set_ylabel("Precision da Classe 2 (%) — Confiabilidade do Alerta")
    axes[1, 1].set_title("(D) Trade-off Precision × Recall da Classe 2 no Modo Zero Contexto")
    axes[1, 1].set_xlim(25, 95)
    axes[1, 1].set_ylim(30, 95)

    plt.tight_layout(rect=[0, 0, 1, 0.96])
    save_fig(fig, "fig04_architecture_benchmark_global.png")

    # =========================================================================
    # FIG 05: All 8 Context Modes Heatmap
    # =========================================================================
    mat_acc = np.array([[results[a][m]["accuracy"]*100 for m in mode_names] for a in arch_names])
    mat_f1 = np.array([[results[a][m]["macro_f1"]*100 for m in mode_names] for a in arch_names])

    fig, axes = plt.subplots(1, 2, figsize=(17, 7.5))
    fig.suptitle("Fig. 05 — Matriz Completa: 10 Arquiteturas × Todos os 8 Modos de Contexto Operacional", fontsize=13.5, fontweight="bold")
    for ax, mat, ttl in [(axes[0], mat_acc, "(A) Acurácia (%) por Modo de Contexto"), (axes[1], mat_f1, "(B) Macro F1-Score (%) por Modo de Contexto")]:
        im = ax.imshow(mat, cmap="YlGnBu", vmin=55, vmax=100, aspect="auto")
        ax.set_xticks(np.arange(8))
        ax.set_xticklabels(short_m)
        ax.set_yticks(np.arange(len(arch_names)))
        ax.set_yticklabels(arch_names if ax == axes[0] else [])
        ax.set_title(ttl)
        for i in range(mat.shape[0]):
            for j in range(mat.shape[1]):
                val = mat[i, j]
                color = "white" if val > 85 else "black"
                ax.text(j, i, f"{val:.1f}", ha="center", va="center", color=color, fontsize=7.8, fontweight="bold")
        fig.colorbar(im, ax=ax, fraction=0.04, pad=0.03)
    plt.tight_layout(rect=[0, 0, 1, 0.94])
    save_fig(fig, "fig05_all_8_context_modes_heatmap.png")

    # =========================================================================
    # FIG 06: Training Dynamics & Convergence
    # =========================================================================
    fig, axes = plt.subplots(2, 2, figsize=(15, 9.5))
    fig.suptitle("Fig. 06 — Dinâmica de Treinamento e Convergência das Redes Neurais TinyML", fontsize=13.5, fontweight="bold")
    colors_h = ["#e74c3c", "#3498db", "#95a5a6", "#9b59b6", "#1abc9c", "#2ecc71", "#f39c12"]
    for (mname, h), col in zip(histories.items(), colors_h):
        ep = np.arange(1, len(h["loss"]) + 1)
        axes[0, 0].plot(ep, h["loss"], color=col, lw=1.8, label=mname)
        axes[0, 1].plot(ep, h["val_loss"], color=col, lw=1.8, label=mname)
        axes[1, 0].plot(ep, np.array(h["accuracy"])*100, color=col, lw=1.8, label=mname)
        axes[1, 1].plot(ep, np.array(h["val_accuracy"])*100, color=col, lw=1.8, label=mname)
    axes[0, 0].set_title("(A) Loss de Treinamento (Crossentropy)")
    axes[0, 1].set_title("(B) Loss de Validação (EarlyStopping Monitor)")
    axes[1, 0].set_title("(C) Acurácia de Treinamento (%)")
    axes[1, 1].set_title("(D) Acurácia de Validação (%)")
    for ax in axes.ravel():
        ax.set_xlabel("Época")
        ax.legend(fontsize=7.5)
    axes[0, 0].set_ylabel("Loss")
    axes[0, 1].set_ylabel("Val Loss")
    axes[1, 0].set_ylabel("Accuracy (%)")
    axes[1, 1].set_ylabel("Val Accuracy (%)")
    plt.tight_layout(rect=[0, 0, 1, 0.96])
    save_fig(fig, "fig06_training_dynamics_and_convergence.png")

    # =========================================================================
    # FIG 07: Confusion Matrices Grid (4 Models x 3 Key Modes)
    # =========================================================================
    fig, axes = plt.subplots(3, 4, figsize=(16, 11.5))
    fig.suptitle("Fig. 07 — Matrizes de Confusão Normalizadas (%) — Comparativo por Modo Operacional", fontsize=13.5, fontweight="bold")
    cm_models = ["MLP-Orig (Buggy)", "MLP-Clean (32-16)", "PhysMLP-27 (+VPD)", "GBDT-Tabular (35x4)"]
    cm_modes = [("M1: Completo [S,C,St]", "M1: Completo"), ("M6: Apenas Cultura [0,C,0]", "M6: Só Cultura"), ("M8: Zero Contexto [0,0,0]", "M8: Zero Contexto")]
    for r_i, (mkey, mshort) in enumerate(cm_modes):
        for c_i, mod_name in enumerate(cm_models):
            ax = axes[r_i, c_i]
            cm = np.array(results[mod_name][mkey]["cm"], dtype=np.float64)
            cm_norm = cm / np.maximum(1, cm.sum(axis=1, keepdims=True)) * 100.0
            ax.imshow(cm_norm, cmap="Blues", vmin=0, vmax=100)
            for i in range(3):
                for j in range(3):
                    txt_col = "white" if cm_norm[i, j] > 55 else "black"
                    ax.text(j, i, f"{cm_norm[i,j]:.1f}%\n({int(cm[i,j])})", ha="center", va="center", color=txt_col, fontsize=7.5)
            ax.set_xticks([0, 1, 2])
            ax.set_xticklabels(["OFF", "IRR", "STR"])
            ax.set_yticks([0, 1, 2])
            ax.set_yticklabels(["OFF", "IRR", "STR"])
            if r_i == 0:
                ax.set_title(mod_name)
            if c_i == 0:
                ax.set_ylabel(f"{mshort}\nReal")
            if r_i == 2:
                ax.set_xlabel("Predito")
    plt.tight_layout(rect=[0, 0, 1, 0.96])
    save_fig(fig, "fig07_confusion_matrices_grid.png")

    # =========================================================================
    # FIG 08: ROC, PR, Calibration (ECE) & Confidence Threshold Tuning
    # =========================================================================
    fig, axes = plt.subplots(2, 2, figsize=(15, 10))
    fig.suptitle("Fig. 08 — Curvas ROC, Precision-Recall (Classe 2), Calibração (ECE) e Threshold Tuning", fontsize=13.5, fontweight="bold")
    comp_models = [("MLP-Orig (Buggy)", "#e74c3c"), ("MLP-Clean (32-16)", "#3498db"), ("PhysMLP-27 (+VPD)", "#2ecc71"), ("GBDT-Tabular (35x4)", "#8e44ad")]

    # (A) ROC Classe 2 (Thermal Stress) no Modo Zero Contexto
    for mname, col in comp_models:
        fpr = arr[f"{mname}_c2_fpr"]
        tpr = arr[f"{mname}_c2_tpr"]
        auc_v = summary["roc_auc"][mname]["c2"]
        axes[0, 0].plot(fpr, tpr, color=col, lw=2, label=f"{mname} (AUC={auc_v:.4f})")
    axes[0, 0].plot([0, 1], [0, 1], "k--", alpha=0.5)
    axes[0, 0].set_title("(A) Curva ROC — Classe 2 (THERMAL_STRESS) em Zero Contexto")
    axes[0, 0].set_xlabel("False Positive Rate (1 - Especificidade)")
    axes[0, 0].set_ylabel("True Positive Rate (Recall)")
    axes[0, 0].legend(loc="lower right")

    # (B) Precision-Recall Classe 2
    for mname, col in comp_models:
        rec = arr[f"{mname}_c2_rec"]
        prec = arr[f"{mname}_c2_prec"]
        auprc_v = summary["auprc"][mname]["c2"]
        axes[0, 1].plot(rec, prec, color=col, lw=2, label=f"{mname} (AUPRC={auprc_v:.4f})")
    axes[0, 1].set_title("(B) Curva Precision-Recall — Classe 2 em Zero Contexto")
    axes[0, 1].set_xlabel("Recall")
    axes[0, 1].set_ylabel("Precision")
    axes[0, 1].legend(loc="lower left")

    # (C) Reliability Diagram (Calibration)
    bins = np.linspace(0.3, 1.0, 8)
    for mname, col in comp_models:
        ym8 = arr[f"{mname}_ym8"]
        pm8 = arr[f"{mname}_pm8"]
        conf = np.max(pm8, axis=1)
        corr = (np.argmax(pm8, axis=1) == ym8).astype(float)
        bx, by = [], []
        for i in range(len(bins) - 1):
            mk = (conf >= bins[i]) & (conf < bins[i+1] + (0.01 if i == len(bins)-2 else 0.0))
            if np.sum(mk) > 5:
                bx.append(np.mean(conf[mk]))
                by.append(np.mean(corr[mk]))
        ece = results[mname]["M8: Zero Contexto [0,0,0]"]["ece"]
        axes[1, 0].plot(bx, by, "o-", color=col, lw=2, label=f"{mname} (ECE={ece:.4f})")
    axes[1, 0].plot([0.3, 1.0], [0.3, 1.0], "k--", label="Calibração Perfeita")
    axes[1, 0].set_title("(C) Diagrama de Confiabilidade / Calibração de Probabilidades")
    axes[1, 0].set_xlabel("Confiança Média Predita")
    axes[1, 0].set_ylabel("Acurácia Real Observada")
    axes[1, 0].legend(loc="upper left")

    # (D) Threshold Tuning para Acionamento da Válvula (IRRIGATE) no ESP32
    ths = np.linspace(0.35, 0.90, 23)
    ym8_p = arr["PhysMLP-27 (+VPD)_ym8"]
    pm8_p = arr["PhysMLP-27 (+VPD)_pm8"]
    irr_prec, irr_rec, irr_f1 = [], [], []
    for th in ths:
        pred_irr = (np.argmax(pm8_p, axis=1) == 1) & (pm8_p[:, 1] >= th)
        true_irr = (ym8_p == 1)
        tp = np.sum(pred_irr & true_irr)
        fp = np.sum(pred_irr & ~true_irr)
        fn = np.sum(~pred_irr & true_irr)
        p = tp / max(1, tp + fp)
        r = tp / max(1, tp + fn)
        irr_prec.append(p * 100)
        irr_rec.append(r * 100)
        irr_f1.append((2 * p * r / max(1e-9, p + r)) * 100)
    axes[1, 1].plot(ths, irr_prec, "o-", color="#27ae60", lw=2, label="Precision (Acionar Válvula)")
    axes[1, 1].plot(ths, irr_rec, "s-", color="#2980b9", lw=2, label="Recall (Acionar Válvula)")
    axes[1, 1].plot(ths, irr_f1, "^-", color="#8e44ad", lw=2, label="F1-Score (Acionar Válvula)")
    axes[1, 1].axvline(0.55, color="#e67e22", ls="--", lw=1.8, label="Threshold Padrão ESP32 (0.55)")
    axes[1, 1].set_title("(D) Tuning do Limiar de Confiança (PhysMLP-27, Modo Zero Contexto)")
    axes[1, 1].set_xlabel("DEFAULT_CONFIDENCE_THRESHOLD")
    axes[1, 1].set_ylabel("Métrica (%)")
    axes[1, 1].legend(loc="lower left")

    plt.tight_layout(rect=[0, 0, 1, 0.96])
    save_fig(fig, "fig08_roc_pr_and_calibration_curves.png")

    # =========================================================================
    # FIG 09: Embedded Pareto (Flash, RAM, Latency, Quantization Sizes)
    # =========================================================================
    fig, axes = plt.subplots(2, 2, figsize=(15, 10))
    fig.suptitle("Fig. 09 — Fronteira de Pareto Embarcada (ESP32): Flash, SRAM, Latência & Quantização", fontsize=13.5, fontweight="bold")

    nn_names = [n for n in arch_names if "float16_bytes" in quant[n]]
    # (A) Flash INT8/C++ vs Macro-F1 (M8)
    for n in arch_names:
        kb = quant[n]["int8_bytes"] / 1024.0
        f1 = results[n]["M8: Zero Contexto [0,0,0]"]["macro_f1"] * 100
        axes[0, 0].scatter(kb, f1, s=110)
        axes[0, 0].annotate(n, (kb, f1), textcoords="offset points", xytext=(5, 3), fontsize=7.5)
    axes[0, 0].set_xlabel("Tamanho do Modelo na Flash (KB - INT8 / Nós C++)")
    axes[0, 0].set_ylabel("Macro F1-Score Zero Contexto (%)")
    axes[0, 0].set_title("(A) Pareto Flash (KB) × Macro F1-Score")

    # (B) Comparativo de Tamanho em Bytes: Notebook Buggy vs Pure Float32 vs Float16 vs Full INT8
    x_q = np.arange(len(nn_names))
    b_nb = [quant[n]["notebook_default_bytes"] for n in nn_names]
    b_f16 = [quant[n]["float16_bytes"] for n in nn_names]
    b_i8 = [quant[n]["int8_bytes"] for n in nn_names]
    axes[0, 1].bar(x_q - 0.25, b_nb, width=0.25, color="#e74c3c", label="Notebook Optimize.DEFAULT (Falso INT8 = Float32!)")
    axes[0, 1].bar(x_q, b_f16, width=0.25, color="#f39c12", label="Float16")
    axes[0, 1].bar(x_q + 0.25, b_i8, width=0.25, color="#2ecc71", label="Full INT8 Real (c/ representative_dataset)")
    axes[0, 1].set_xticks(x_q)
    axes[0, 1].set_xticklabels(nn_names, rotation=20, ha="right")
    axes[0, 1].set_ylabel("Tamanho do Arquivo .tflite (Bytes)")
    axes[0, 1].set_title("(B) Bug #8: Falsa Quantização do Notebook vs. Full INT8 Real")
    axes[0, 1].legend(fontsize=7.5)

    # (C) Estimativa de Latência de Inferência no ESP32 (240 MHz) vs Acurácia M1
    # Estimativa analítica baseada em MACs (~2.5 ciclos/MAC em INT8/float32 ESP32, árvores ~0.15 us/profundidade)
    est_us = []
    for n in arch_names:
        if "CART" in n: est_us.append(1.8)
        elif "RandomForest" in n: est_us.append(18.0)
        elif "GBDT" in n: est_us.append(48.0)
        else:
            p = quant[n]["params"]
            est_us.append(max(8.0, p * 0.045))
    for n, us in zip(arch_names, est_us):
        acc1 = results[n]["M1: Completo [S,C,St]"]["accuracy"] * 100
        axes[1, 0].scatter(us, acc1, s=110)
        axes[1, 0].annotate(n, (us, acc1), textcoords="offset points", xytext=(5, 3), fontsize=7.5)
    axes[1, 0].set_xscale("log")
    axes[1, 0].set_xlabel("Latência Estimada no ESP32 @ 240 MHz (µs, escala log)")
    axes[1, 0].set_ylabel("Acurácia Contexto Completo (%)")
    axes[1, 0].set_title("(C) Latência Estimada ESP32 (µs) × Acurácia M1")

    # (D) Memória SRAM Estimada (tensor_arena + overhead TFLM vs C++ puro)
    ram_kb = []
    for n in arch_names:
        if "CART" in n or "RandomForest" in n: ram_kb.append(0.3)
        elif "GBDT" in n: ram_kb.append(0.6)
        elif "Nano" in n: ram_kb.append(2.8)
        elif "Deep" in n: ram_kb.append(5.6)
        else: ram_kb.append(3.6)
    axes[1, 1].barh(np.arange(len(arch_names)), ram_kb, color=["#1abc9c" if r < 1.0 else "#3498db" for r in ram_kb])
    axes[1, 1].set_yticks(np.arange(len(arch_names)))
    axes[1, 1].set_yticklabels(arch_names)
    axes[1, 1].set_xlabel("Consumo Estimado de SRAM / tensor_arena (KB)")
    axes[1, 1].set_title("(D) Footprint de SRAM Estimado no ESP32")

    plt.tight_layout(rect=[0, 0, 1, 0.96])
    save_fig(fig, "fig09_embedded_pareto_flash_ram_latency.png")

    # =========================================================================
    # FIG 10: Quantization Fidelity (Keras Float32 vs Full INT8)
    # =========================================================================
    fig, axes = plt.subplots(2, 2, figsize=(15, 9.5))
    fig.suptitle("Fig. 10 — Fidelidade Numérica da Quantização Full INT8 (PhysMLP-27)", fontsize=13.5, fontweight="bold")

    diffs = qdet["abs_diffs"]
    pk = qdet["p_keras"].ravel()
    pi = qdet["p_int8"].ravel()

    axes[0, 0].hist(diffs, bins=35, color="#3498db", edgecolor="white", alpha=0.85)
    axes[0, 0].axvline(np.mean(diffs), color="#e74c3c", ls="--", lw=2, label=f"Média = {np.mean(diffs):.5f}")
    axes[0, 0].axvline(np.max(diffs), color="#8e44ad", ls=":", lw=2, label=f"Máx = {np.max(diffs):.5f}")
    axes[0, 0].set_xlabel("|Prob(Keras Float32) - Prob(TFLite INT8)|")
    axes[0, 0].set_ylabel("Frequência")
    axes[0, 0].set_title("(A) Distribuição do Erro Absoluto de Probabilidade Softmax")
    axes[0, 0].legend()

    axes[0, 1].scatter(pk, pi, s=8, alpha=0.45, color="#2ecc71")
    axes[0, 1].plot([0, 1], [0, 1], "r--", lw=1.5)
    axes[0, 1].set_xlabel("Probabilidade Keras (Float32)")
    axes[0, 1].set_ylabel("Probabilidade TFLite (Full INT8)")
    axes[0, 1].set_title("(B) Correlação Keras Float32 vs. TFLite Full INT8")

    # (C) Taxa de concordância entre modelos neurais
    agrees = [quant[n]["decision_agreement"]*100 for n in nn_names]
    max_ds = [quant[n]["max_abs_diff"] for n in nn_names]
    axes[1, 0].barh(np.arange(len(nn_names)), agrees, color="#27ae60")
    axes[1, 0].set_yticks(np.arange(len(nn_names)))
    axes[1, 0].set_yticklabels(nn_names)
    axes[1, 0].set_xlim(95, 100.5)
    axes[1, 0].set_xlabel("Concordância de Decisão Keras vs. INT8 (%)")
    axes[1, 0].set_title("(C) Taxa de Concordância Argmax após Quantização INT8")
    for i, v in enumerate(agrees):
        axes[1, 0].text(v - 0.1, i, f"{v:.2f}%", va="center", ha="right", color="white", fontweight="bold", fontsize=8)

    # (D) Erro máximo absoluto por arquitetura neural
    axes[1, 1].barh(np.arange(len(nn_names)), max_ds, color="#e67e22")
    axes[1, 1].set_yticks(np.arange(len(nn_names)))
    axes[1, 1].set_yticklabels(nn_names)
    axes[1, 1].set_xlabel("Diferença Máxima Absoluta de Probabilidade")
    axes[1, 1].set_title("(D) Pior Desvio Numérico INT8 por Arquitetura")

    plt.tight_layout(rect=[0, 0, 1, 0.96])
    save_fig(fig, "fig10_quantization_fidelity_and_error_distribution.png")

    # =========================================================================
    # FIG 11: Feature Importance & Physical Ablation
    # =========================================================================
    fig, axes = plt.subplots(1, 2, figsize=(15.5, 6.5))
    fig.suptitle("Fig. 11 — Interpretabilidade: Permutation Feature Importance & Ablação Física", fontsize=13.5, fontweight="bold")

    fnames = np.array(summary["feat_names"])
    pimp = np.array(summary["perm_imp"]) * 100.0
    order = np.argsort(pimp)
    top_idx = order[-16:]
    axes[0].barh(np.arange(len(top_idx)), pimp[top_idx], color=["#e74c3c" if "VPD" in fnames[i] or "Temp" in fnames[i] or "MOI" in fnames[i] or "Evap" in fnames[i] else "#3498db" for i in top_idx])
    axes[0].set_yticks(np.arange(len(top_idx)))
    axes[0].set_yticklabels(fnames[top_idx])
    axes[0].set_xlabel("Queda de Acurácia na Permutação (pontos %)")
    axes[0].set_title("(A) Top 16 Features Mais Importantes (PhysMLP-27, Modo 1)")

    # (B) Comparativo direto MLP-Clean (23 inputs) vs PhysMLP-27 (+4 features físicas)
    m_comp = ["M1: Completo [S,C,St]", "M4: Cultura+Estágio [0,C,St]", "M6: Apenas Cultura [0,C,0]", "M8: Zero Contexto [0,0,0]"]
    c23_f1 = [results["MLP-Clean (32-16)"][m]["macro_f1"]*100 for m in m_comp]
    p27_f1 = [results["PhysMLP-27 (+VPD)"][m]["macro_f1"]*100 for m in m_comp]
    xab = np.arange(len(m_comp))
    axes[1].bar(xab - 0.18, c23_f1, width=0.35, color="#3498db", label="23 Inputs (Sensores Crus + One-Hot)")
    axes[1].bar(xab + 0.18, p27_f1, width=0.35, color="#2ecc71", label="27 Inputs (+VPD, +EvapDemand, +T/MOI, +MOI*T)")
    axes[1].set_xticks(xab)
    axes[1].set_xticklabels(["M1\nCompleto", "M4\nCult+Est", "M6\nSó Cult", "M8\nZero Ctx"])
    axes[1].set_ylim(60, 102)
    axes[1].set_ylabel("Macro F1-Score (%)")
    axes[1].set_title("(B) Ganho da Engenharia de Atributos Psicrométricos")
    axes[1].legend(loc="lower left")
    for i in range(len(m_comp)):
        diff_pp = p27_f1[i] - c23_f1[i]
        axes[1].text(i + 0.18, p27_f1[i] + 0.6, f"{diff_pp:+.2f}pp", ha="center", fontsize=8, fontweight="bold", color="#1e8449")

    plt.tight_layout(rect=[0, 0, 1, 0.94])
    save_fig(fig, "fig11_feature_importance_and_ablation.png")

    # =========================================================================
    # FIG 12: 48h ESP32 Field Simulation + Sensor Fault Injection
    # =========================================================================
    fig, axes = plt.subplots(3, 1, figsize=(15, 10.5), sharex=True)
    fig.suptitle("Fig. 12 — Simulação 48h no ESP32 (Paulista-PE) & Injeção de Falha DHT22 (-128 °C / 255%)", fontsize=13.5, fontweight="bold")

    hrs = arr["hours"]
    stemp = arr["sim_temp"]
    shum = arr["sim_hum"]
    smoi = arr["sim_moi"]
    fmask = arr["fault_mask"]
    po = arr["p_sim_orig"]
    pp_raw = arr["p_sim_phys_raw"]
    pp_safe = arr["p_sim_phys_safe"]

    # Plot 1: Leituras dos sensores (clipando visualmente a falha com anotação)
    stemp_vis = np.where(fmask, np.nan, stemp)
    shum_vis = np.where(fmask, np.nan, shum)
    axes[0].plot(hrs, stemp_vis, color="#e74c3c", lw=2, label="Temperatura Ar (°C)")
    axes[0].plot(hrs, shum_vis, color="#3498db", lw=2, label="Umidade Ar (%)")
    axes[0].plot(hrs, smoi, color="#27ae60", lw=2.2, label="Umidade Solo HW-080 (%)")
    axes[0].axvspan(34, 36, color="#e74c3c", alpha=0.22, label="Falha DHT22 (temp=-128, hum=255)")
    axes[0].set_ylabel("Leitura Física")
    axes[0].set_title("(A) Telemetria Simulada de Campo (Irrigação às 26h + Falha de Sensor 34h-36h)")
    axes[0].legend(loc="upper right", ncol=2)

    # Plot 2: Probabilidades do Modelo Original (Buggy)
    axes[1].plot(hrs, po[:, 0], color=C_OFF, lw=1.8, label="P(0: OFF)")
    axes[1].plot(hrs, po[:, 1], color=C_IRR, lw=1.8, label="P(1: IRRIGATE)")
    axes[1].plot(hrs, po[:, 2], color=C_STR, lw=2.0, label="P(2: THERMAL_STRESS)")
    axes[1].axvspan(34, 36, color="#e74c3c", alpha=0.18)
    axes[1].set_ylabel("Probabilidade")
    axes[1].set_title("(B) Resposta do Modelo Original (Buggy): Falsos Positivos de Classe 2 + Sem Proteção de Sentinela")
    axes[1].legend(loc="upper right", ncol=3)

    # Plot 3: Probabilidades do PhysMLP-27 + Proteção de Sentinela
    axes[2].plot(hrs, pp_safe[:, 0], color=C_OFF, lw=1.8, label="P(0: OFF / SAFE)")
    axes[2].plot(hrs, pp_safe[:, 1], color=C_IRR, lw=1.8, label="P(1: IRRIGATE)")
    axes[2].plot(hrs, pp_safe[:, 2], color=C_STR, lw=2.0, label="P(2: THERMAL_STRESS)")
    axes[2].axvspan(34, 36, color="#e74c3c", alpha=0.18, label="Fail-Safe Ativado (Abortar Inferência)")
    axes[2].set_xlabel("Tempo de Operação (Horas)")
    axes[2].set_ylabel("Probabilidade")
    axes[2].set_title("(C) Resposta PhysMLP-27 (Cultura=Tomato) + Validação de Sentinela no Firmware")
    axes[2].legend(loc="upper right", ncol=4)

    plt.tight_layout(rect=[0, 0, 1, 0.96])
    save_fig(fig, "fig12_esp32_stress_simulation_24h.png")
    print("Todas as 12 figuras geradas com sucesso!")


if __name__ == "__main__":
    main()
