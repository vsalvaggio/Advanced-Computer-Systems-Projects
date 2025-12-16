# plot_hash_benchmark.py
import os
import glob
import pandas as pd
import matplotlib.pyplot as plt

RESULTS_DIR = "results"
OUTPUT_DIR = "plots"

os.makedirs(OUTPUT_DIR, exist_ok=True)

# Find all CSV files
csv_files = glob.glob(os.path.join(RESULTS_DIR, "*.csv"))

# Combine all CSVs into a single DataFrame
all_data = []
for csv_file in csv_files:
    df = pd.read_csv(csv_file)
    all_data.append(df)
data = pd.concat(all_data, ignore_index=True)

# Convert Threads column to int just in case
data['Threads'] = data['Threads'].astype(int)

# Unique workloads and dataset sizes
workloads = data['Workload'].unique()
datasets = sorted(data['Dataset'].unique())

# Plot throughput vs threads for each workload & dataset
for workload in workloads:
    for dataset in datasets:
        subset = data[(data['Workload'] == workload) & (data['Dataset'] == dataset)]
        if subset.empty:
            continue

        plt.figure(figsize=(8,6))
        plt.plot(subset['Threads'], subset['CoarseTput'], marker='o', label='Coarse-Grained')
        plt.plot(subset['Threads'], subset['FineTput'], marker='s', label='Fine-Grained')
        plt.xlabel("Number of Threads")
        plt.ylabel("Throughput (ops/s)")
        plt.title(f"{workload} - Dataset {dataset} keys")
        plt.xscale("log", base=2)
        plt.xticks(subset['Threads'], subset['Threads'])
        plt.legend()
        plt.grid(True, which="both", ls="--", alpha=0.5)
        plt.tight_layout()

        # Save figure
        filename = f"{workload}_{dataset}_throughput.png"
        plt.savefig(os.path.join(OUTPUT_DIR, filename), dpi=200)
        plt.close()
        print(f"✅ Saved plot: {filename}")

# Optional: Plot speedup vs threads
for workload in workloads:
    for dataset in datasets:
        subset = data[(data['Workload'] == workload) & (data['Dataset'] == dataset)]
        if subset.empty:
            continue

        plt.figure(figsize=(8,6))
        baseline_coarse = subset['CoarseTput'].iloc[0]
        baseline_fine   = subset['FineTput'].iloc[0]
        plt.plot(subset['Threads'], subset['CoarseTput']/baseline_coarse, marker='o', label='Coarse-Grained')
        plt.plot(subset['Threads'], subset['FineTput']/baseline_fine, marker='s', label='Fine-Grained')
        plt.xlabel("Number of Threads")
        plt.ylabel("Speedup vs 1 Thread")
        plt.title(f"{workload} - Dataset {dataset} keys (Speedup)")
        plt.xscale("log", base=2)
        plt.xticks(subset['Threads'], subset['Threads'])
        plt.legend()
        plt.grid(True, which="both", ls="--", alpha=0.5)
        plt.tight_layout()

        # Save figure
        filename = f"{workload}_{dataset}_speedup.png"
        plt.savefig(os.path.join(OUTPUT_DIR, filename), dpi=200)
        plt.close()
        print(f"✅ Saved plot: {filename}")
