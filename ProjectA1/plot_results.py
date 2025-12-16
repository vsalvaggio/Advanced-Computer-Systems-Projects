#!/usr/bin/env python3
"""
ECSE 4320 Project A1 — Automated Plot Generator
Reads all CSV files in ./results/ and generates bar plots for runtime comparisons.
"""

import os
import pandas as pd
import matplotlib.pyplot as plt

# === Settings ===
RESULTS_DIR = "results"
OUTPUT_DIR = "plots"
os.makedirs(OUTPUT_DIR, exist_ok=True)

def plot_csv(csv_path):
    # Get the benchmark name from filename
    benchmark = os.path.splitext(os.path.basename(csv_path))[0]
    df = pd.read_csv(csv_path)

    # Basic sanity check
    if 'Mode' not in df.columns and 'THP_Mode' in df.columns:
        df.rename(columns={'THP_Mode': 'Mode'}, inplace=True)

    # Compute average runtime per mode
    grouped = df.groupby('Mode', as_index=False)['Time_s'].mean().sort_values('Time_s')
    grouped['Error'] = df.groupby('Mode')['Time_s'].std().reindex(grouped['Mode']).values

    # === Plot ===
    plt.figure(figsize=(6, 4))
    bars = plt.bar(grouped['Mode'], grouped['Time_s'], yerr=grouped['Error'],
                   capsize=5, color="#4C72B0", alpha=0.85)
    plt.xlabel("Mode", fontsize=11)
    plt.ylabel("Average Runtime (s)", fontsize=11)
    plt.title(f"{benchmark.replace('_',' ').title()} Benchmark", fontsize=13, weight='bold')
    plt.grid(axis='y', linestyle='--', alpha=0.5)

    # Annotate bar values
    for bar, val in zip(bars, grouped['Time_s']):
        plt.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.02,
                 f"{val:.2f}", ha='center', va='bottom', fontsize=9)

    plt.tight_layout()
    out_path = os.path.join(OUTPUT_DIR, f"{benchmark}.png")
    plt.savefig(out_path, dpi=200)
    plt.close()
    print(f"✅ Saved plot: {out_path}")

def main():
    csv_files = [f for f in os.listdir(RESULTS_DIR) if f.endswith(".csv")]
    if not csv_files:
        print("⚠️ No CSV files found in ./results/. Run ./run_all.sh first.")
        return

    for csv in csv_files:
        plot_csv(os.path.join(RESULTS_DIR, csv))

    print("\nAll plots generated in the 'plots/' folder.")

if __name__ == "__main__":
    main()
