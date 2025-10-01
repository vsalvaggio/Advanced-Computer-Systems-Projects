import pandas as pd # type: ignore
import matplotlib.pyplot as plt # type: ignore
import seaborn as sns # type: ignore
import os
import numpy as np # type: ignore

# Style
sns.set(style="whitegrid", font_scale=1.2)

# Paths
CSV_DIR = "csv"
PLOT_DIR = "plots"
os.makedirs(PLOT_DIR, exist_ok=True)

###############################################
# 1. Cache latencies (line graph, ordered L1→DRAM)
###############################################
df = pd.read_csv(f"{CSV_DIR}/cache_latencies.csv", sep="\t|,", engine="python")
order = ["L1", "L2", "L3", "DRAM"]
df["cache"] = pd.Categorical(df["cache"], categories=order, ordered=True)
df = df.sort_values("cache")

plt.figure(figsize=(6,4))
plt.plot(df["cache"], df["ns"], marker="o", linestyle="-")
plt.ylabel("Latency (ns)")
plt.title("Zero-Queue Cache & DRAM Latencies")
plt.tight_layout()
plt.savefig(f"{PLOT_DIR}/cache_latencies.png")
plt.close()

###############################################
# 2. Loaded Latency sweep
###############################################
df = pd.read_csv(f"{CSV_DIR}/loaded_latency_1to1RW.csv", sep="\t|,", engine="python")

plt.figure(figsize=(7,5))
for size, group in df.groupby("size_MiB"):
    plt.plot(group["Inject_Delay"], group["latency_ns"], marker="o", label=f"{size} MiB")
plt.xlabel("Inject Delay")
plt.ylabel("Latency (ns)")
plt.title("Loaded Latency Sweep (1:1 R/W)")
plt.legend()
plt.tight_layout()
plt.savefig(f"{PLOT_DIR}/loaded_latency.png")
plt.close()

loaded_table = df.groupby("size_MiB")[["Bandwidth_MB/s", "latency_ns"]].mean()
print("\n### Loaded Latency Table (Mean Bandwidth & Latency) ###")
print(loaded_table.to_markdown())

###############################################
# 3+4. Overlay Latency & Bandwidth vs stride
###############################################
df_bw = pd.read_csv(f"{CSV_DIR}/bandwidth_stride_seq.csv")
df_lat = pd.read_csv(f"{CSV_DIR}/latency_stride_seq.csv")

def plot_stride_overlay(lat_col="ns", bw_col="1:1RW_MB/s", title_suffix="1:1 R/W", filename="stride_latency_bandwidth_overlay.png"):
    df_merge = pd.merge(df_lat, df_bw[["stride", bw_col]], on="stride")
    fig, ax1 = plt.subplots(figsize=(7,5))
    ax1.plot(df_merge["stride"], df_merge[lat_col], color="tab:blue", marker="o", label="Latency (ns)")
    ax1.set_xscale("log", base=2)
    ax1.set_xlabel("Stride (bytes)")
    ax1.set_ylabel("Latency (ns)", color="tab:blue")
    ax1.tick_params(axis="y", labelcolor="tab:blue")

    ax2 = ax1.twinx()
    ax2.bar(df_merge["stride"], df_merge[bw_col], width=0.3*df_merge["stride"], color="tab:orange", alpha=0.6, label="Bandwidth (MB/s)")
    ax2.set_ylabel("Bandwidth (MB/s)", color="tab:orange")
    ax2.tick_params(axis="y", labelcolor="tab:orange")

    plt.title(f"Latency & Bandwidth Across Strides ({title_suffix})")
    fig.tight_layout()
    plt.savefig(f"{PLOT_DIR}/{filename}")
    plt.close()

# 1:1 R/W (existing)
plot_stride_overlay()

# 2:1 R/W
plot_stride_overlay(bw_col="2:1RW_MB/s", title_suffix="2:1 R/W", filename="stride_latency_bandwidth_overlay_2to1RW.png")

# 3:1 R/W
plot_stride_overlay(bw_col="3:1RW_MB/s", title_suffix="3:1 R/W", filename="stride_latency_bandwidth_overlay_3to1RW.png")

###############################################
# 5. Read/Write Mix (show only means)
###############################################
df = pd.read_csv(f"{CSV_DIR}/Read_Write_Mix_latency_bw.csv")
df["ReadPercent"] = 100 * df["Reads"] / (df["Reads"] + df["Writes"])
df_mean = df.groupby("ReadPercent")[["Bandwidth_MB/s","Latency_ns"]].mean().reset_index()

plt.figure(figsize=(7,5))
plt.plot(df_mean["ReadPercent"], df_mean["Bandwidth_MB/s"], marker="o", linestyle="-")
plt.title("Read/Write Mix: Mean Bandwidth vs % Reads")
plt.xlabel("% Reads")
plt.ylabel("Bandwidth (MB/s)")
plt.tight_layout()
plt.savefig(f"{PLOT_DIR}/rw_bandwidth.png")
plt.close()

plt.figure(figsize=(7,5))
plt.plot(df_mean["ReadPercent"], df_mean["Latency_ns"], marker="o", linestyle="-")
plt.title("Read/Write Mix: Mean Latency vs % Reads")
plt.xlabel("% Reads")
plt.ylabel("Latency (ns)")
plt.tight_layout()
plt.savefig(f"{PLOT_DIR}/rw_latency.png")
plt.close()

###############################################
# 6. Intensity sweep (Latency vs Bandwidth, lines per thread)
###############################################
df = pd.read_csv(f"{CSV_DIR}/intensity_loaded_latency_threads.csv")
plt.figure(figsize=(7,5))
for thr, group in df.groupby("threads"):
    group_sorted = group.sort_values("Bandwidth_MB/s")
    plt.scatter(group_sorted["Bandwidth_MB/s"], group_sorted["Latency_ns"], marker="o", label=f"{thr} threads")
plt.xlabel("Bandwidth (MB/s)")
plt.ylabel("Latency (ns)")
plt.title("Throughput vs Latency (Threads)")
plt.legend()
plt.tight_layout()
plt.savefig(f"{PLOT_DIR}/intensity_latency_bandwidth.png")
plt.close()

###############################################
# 7. Working-set size sweep
###############################################
df = pd.read_csv(f"{CSV_DIR}/working_set_sweep.csv")
plt.figure(figsize=(6,4))
plt.plot(df["size_KB"], df["ns"], marker="o")
plt.xscale("log", base=2)
plt.xlabel("Working Set Size (KB)")
plt.ylabel("Latency (ns)")
plt.title("Working Set Size Sweep (Cache → DRAM)")
plt.tight_layout()
plt.savefig(f"{PLOT_DIR}/working_set.png")
plt.close()

###############################################
# 8. Cache-miss performance (line of best fit)
###############################################
df = pd.read_csv(f"{CSV_DIR}/cache_miss_performance.csv")
plt.figure(figsize=(7,5))
plt.scatter(df["caches_misses"], df["time_elapsed_seconds"], label="Data")
m, b = np.polyfit(df["caches_misses"], df["time_elapsed_seconds"], 1)
plt.plot(df["caches_misses"], m*df["caches_misses"]+b, color="red", label="Best fit")
plt.xlabel("Cache Misses")
plt.ylabel("Elapsed Time (s)")
plt.title("Impact of Cache Misses on Runtime")
plt.legend()
plt.tight_layout()
plt.savefig(f"{PLOT_DIR}/cache_miss_perf.png")
plt.close()

###############################################
# 9. TLB-miss performance (ratio + best fit)
###############################################
df = pd.read_csv(f"{CSV_DIR}/tlb_miss_performance.csv")
df["miss_ratio"] = df["tlb_load_misses"] / df["tlb_loads"]
plt.figure(figsize=(7,5))
plt.scatter(df["miss_ratio"], df["time_elapsed_seconds"], label="Data")
m, b = np.polyfit(df["miss_ratio"], df["time_elapsed_seconds"], 1)
plt.plot(df["miss_ratio"], m*df["miss_ratio"]+b, color="red", label="Best fit")
plt.xlabel("TLB Miss Ratio (misses/loads)")
plt.ylabel("Elapsed Time (s)")
plt.title("Impact of TLB Misses on Runtime")
plt.legend()
plt.tight_layout()
plt.savefig(f"{PLOT_DIR}/tlb_miss_perf.png")
plt.close()

print(f"✅ Plots saved in {PLOT_DIR}/")