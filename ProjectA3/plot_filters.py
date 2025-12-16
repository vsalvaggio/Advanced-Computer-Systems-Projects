#!/usr/bin/env python3
"""
ECSE 4320 Project A3 — Approximate Membership Filters Analysis
Updated version: handles multiple sizes, FPRs, workloads, neg_shares, load_factors, and thread counts.

Generates:
1. Space vs Accuracy (BPE vs FPR_Target)
2. Lookup Throughput vs Negative Lookup Share
3. Insert Throughput vs Load Factor (Dynamic filters)
4. Thread Scaling (Throughput vs Threads)
5. Optional scaling by dataset size
"""

import os
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

# === Settings ===
RESULTS_FILE = "results/all_results.csv"
OUTPUT_DIR = "plots"
os.makedirs(OUTPUT_DIR, exist_ok=True)

# === Load & Prepare Data ===
df = pd.read_csv(RESULTS_FILE)
df["Query_throughput"] = df["N"] / df["Query_s"]
df["Insert_throughput"] = df["N"] / df["Insert_s"]

# Ensure consistent filter order
filter_order = ["bloom", "xor", "cuckoo", "quotient"] if "quotient" in df["Filter"].unique() else ["bloom", "xor", "cuckoo"]

# Color map for filters
colors = {
    "bloom": "#4C72B0",
    "xor": "#55A868",
    "cuckoo": "#C44E52",
    "quotient": "#8172B2",
}

# ============================================================
# 1. Space vs Accuracy (BPE vs FPR_Target)
# ============================================================
def plot_space_vs_accuracy():
    plt.figure(figsize=(8,6))
    for f in filter_order:
        # skip Quotient filter
        if f == "quotient" or f not in df["Filter"].unique():
            continue
        sub = df[df["Filter"] == f]
        grouped = sub.groupby("FPR_Target", as_index=False)[["BPE", "FalsePosRate"]].mean()
        plt.plot(
            grouped["FalsePosRate"], 
            grouped["BPE"], 
            marker='o', 
            label=f, 
            color=colors.get(f, None)
        )
    plt.xscale("log")
    plt.xlabel("False Positive Rate (log scale)", fontsize=11)
    plt.ylabel("Bits per Entry (BPE)", fontsize=11)
    plt.title("Space vs Accuracy (Bloom, XOR, Cuckoo)", fontsize=13, weight="bold")
    plt.legend()
    plt.grid(True, which="both", ls="--", alpha=0.5)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, "space_vs_accuracy.png"), dpi=200)
    plt.close()
    print("✅ Saved: space_vs_accuracy.png")


# ============================================================
# 2. Lookup Throughput vs Negative Lookup Share
# ============================================================
def plot_lookup_throughput():
    plt.figure(figsize=(8,6))
    for f in filter_order:
        sub = df[(df["Filter"] == f) & (df["Workload"] == "read-only")]
        grouped = sub.groupby("NegShare", as_index=False)["Query_throughput"].mean()
        plt.plot(grouped["NegShare"] * 100, grouped["Query_throughput"], marker='o', label=f, color=colors.get(f, None))
    plt.xlabel("Negative Lookup Share (%)", fontsize=11)
    plt.ylabel("Query Throughput (ops/s)", fontsize=11)
    plt.title("Lookup Throughput vs Negative Lookup Share", fontsize=13, weight="bold")
    plt.legend()
    plt.grid(True, ls="--", alpha=0.5)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, "lookup_throughput.png"), dpi=200)
    plt.close()
    print("✅ Saved: lookup_throughput.png")

# ============================================================
# 3. Insert/Delete Throughput vs Load Factor (Dynamic Filters)
# ============================================================
def plot_insert_delete_throughput():
    plt.figure(figsize=(8,6))
    dynamic_filters = ["cuckoo", "quotient"]
    for f in dynamic_filters:
        if f not in df["Filter"].unique():
            continue
        sub = df[(df["Filter"] == f) & (df["Workload"] != "read-only")]
        grouped = sub.groupby("LoadFactor", as_index=False)["Insert_throughput"].mean()
        plt.plot(grouped["LoadFactor"], grouped["Insert_throughput"], marker='o', label=f, color=colors.get(f, None))
    plt.xlabel("Load Factor", fontsize=11)
    plt.ylabel("Insert Throughput (ops/s)", fontsize=11)
    plt.title("Insert/Delete Throughput vs Load Factor (Dynamic Filters)", fontsize=13, weight="bold")
    plt.legend()
    plt.grid(True, ls="--", alpha=0.5)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, "insert_delete_throughput.png"), dpi=200)
    plt.close()
    print("✅ Saved: insert_delete_throughput.png")

# ============================================================
# 4. Thread Scaling (Throughput vs Threads)
# ============================================================
def plot_thread_scaling():
    plt.figure(figsize=(8,6))
    for f in filter_order:
        sub = df[(df["Filter"] == f) & (df["Workload"] == "read-mostly")]
        grouped = sub.groupby("Threads", as_index=False)["Query_throughput"].mean()
        plt.plot(grouped["Threads"], grouped["Query_throughput"], marker='o', label=f, color=colors.get(f, None))
    plt.xlabel("Threads", fontsize=11)
    plt.ylabel("Query Throughput (ops/s)", fontsize=11)
    plt.title("Thread Scaling (Read-Mostly Workload)", fontsize=13, weight="bold")
    plt.legend()
    plt.grid(True, ls="--", alpha=0.5)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, "thread_scaling.png"), dpi=200)
    plt.close()
    print("✅ Saved: thread_scaling.png")

# ============================================================
# 5. Optional: Throughput vs Dataset Size
# ============================================================
def plot_size_scaling():
    plt.figure(figsize=(8,6))
    for f in filter_order:
        sub = df[(df["Filter"] == f) & (df["Workload"] == "read-only") & (df["NegShare"] == 0)]
        grouped = sub.groupby("N", as_index=False)["Query_throughput"].mean()
        plt.plot(grouped["N"] / 1e6, grouped["Query_throughput"], marker='o', label=f, color=colors.get(f, None))
    plt.xlabel("Dataset Size (Million Keys)", fontsize=11)
    plt.ylabel("Query Throughput (ops/s)", fontsize=11)
    plt.title("Throughput vs Dataset Size (Read-Only, No Negatives)", fontsize=13, weight="bold")
    plt.legend()
    plt.grid(True, ls="--", alpha=0.5)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, "throughput_vs_size.png"), dpi=200)
    plt.close()
    print("✅ Saved: throughput_vs_size.png")

# ============================================================
# Main
# ============================================================
if __name__ == "__main__":
    print(f"Loaded {len(df)} rows from {RESULTS_FILE}")
    plot_space_vs_accuracy()
    plot_lookup_throughput()
    plot_insert_delete_throughput()
    plot_thread_scaling()
    plot_size_scaling()
    print("✅ All plots generated in ./plots/")
