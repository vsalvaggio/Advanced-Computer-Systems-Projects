#!/usr/bin/env python3
"""
ECSE 4320 Project A1 — Paired Speedup Plot Generator
Computes speedup between paired modes in each benchmark CSV and plots 4 bars.
"""

import os
import pandas as pd
import matplotlib.pyplot as plt

RESULTS_DIR = "results"
OUTPUT_DIR = "plots"
os.makedirs(OUTPUT_DIR, exist_ok=True)

def compute_paired_speedup(csv_path):
    """
    For each CSV, compute average speedup between the two modes.
    speedup = slower_time / faster_time
    Returns: (benchmark_name, avg_speedup)
    """
    df = pd.read_csv(csv_path)
    benchmark = os.path.splitext(os.path.basename(csv_path))[0]

    # Group by Run, compute speedup per run
    speedups = []
    for run, group in df.groupby("Run"):
        times = group.set_index("Mode")["Time_s"]
        if len(times) != 2:
            raise ValueError(f"Run {run} in {csv_path} does not have exactly 2 modes")
        faster = times.min()
        slower = times.max()
        speedups.append(slower / faster)

    avg_speedup = sum(speedups) / len(speedups)
    return benchmark, avg_speedup

def main():
    csv_files = [f for f in os.listdir(RESULTS_DIR) if f.endswith(".csv")]
    if not csv_files:
        print("⚠️ No CSV files found in ./results/. Run ./run_all.sh first.")
        return

    benchmarks = []
    speedup_values = []

    for csv in csv_files:
        benchmark, avg_speedup = compute_paired_speedup(os.path.join(RESULTS_DIR, csv))
        benchmarks.append(benchmark)
        speedup_values.append(avg_speedup)

    # === Plotting ===
    plt.figure(figsize=(8,6))
    bars = plt.bar(benchmarks, speedup_values, color="#4C72B0", alpha=0.85)
    plt.ylabel("Average Speedup (slower/faster)", fontsize=12)
    plt.title("Paired Speedup Across Benchmarks", fontsize=14, weight='bold')
    plt.grid(axis='y', linestyle='--', alpha=0.5)

    # Annotate bars with speedup value
    for bar, val in zip(bars, speedup_values):
        plt.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.02,
                 f"{val:.2f}x", ha='center', va='bottom', fontsize=10, weight='bold')

    plt.tight_layout()
    out_path = os.path.join(OUTPUT_DIR, "paired_speedup.png")
    plt.savefig(out_path, dpi=200)
    plt.close()
    print(f"✅ Saved paired speedup plot: {out_path}")

if __name__ == "__main__":
    main()
