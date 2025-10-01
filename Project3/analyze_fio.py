#!/usr/bin/env python3
import json, glob, os
import pandas as pd  # type: ignore
import matplotlib.pyplot as plt  # type: ignore

def load_fio(path):
    with open(path) as f:
        j = json.load(f)
    job = j['jobs'][0]
    rec = {
        "file": os.path.basename(path),
        "jobname": job["jobname"],
        "rw": job["job options"].get("rw"),
        "bs": job["job options"].get("bs"),
        "iodepth": int(job["job options"].get("iodepth", 0)),
        "numjobs": int(job["job options"].get("numjobs", 0)),
    }
    for mode in ["read","write"]:
        data = job[mode]
        if data["io_bytes"] > 0:
            rec[f"{mode}_iops"] = data["iops"]
            rec[f"{mode}_bw_kBps"] = data["bw"]  # KB/s
            rec[f"{mode}_lat_mean_us"] = data["clat_ns"]["mean"]/1000
            pct = data["clat_ns"].get("percentile", {})
            for p in ["50.000000","95.000000","99.000000","99.900000"]:
                if p in pct:
                    rec[f"{mode}_p{p.split('.')[0]}_us"] = pct[p]/1000
    return rec

# Load results
files = glob.glob("results/*.json")
rows = [load_fio(f) for f in files]
df = pd.DataFrame(rows)

os.makedirs("plots", exist_ok=True)

# -------- 1. Zero-queue baselines --------
zeroq = df[df['jobname'].str.startswith("zeroq")]
print("\nZero-queue baselines:")
print(zeroq)

# Bandwidth comparison (MB/s)
plt.figure()
zeroq_sorted = zeroq.sort_values("jobname")
for _, row in zeroq_sorted.iterrows():
    label = row["jobname"]
    if pd.notna(row.get("read_bw_kBps")):
        plt.bar(label, row["read_bw_kBps"]/1024, color="blue")
    if pd.notna(row.get("write_bw_kBps")):
        plt.bar(label, row["write_bw_kBps"]/1024, color="orange")
plt.ylabel("Bandwidth (MB/s)")
plt.title("Zero-queue: Bandwidth comparison")
plt.xticks(rotation=30)
plt.tight_layout()
plt.savefig("plots/zeroq_bandwidth.png", dpi=150)

# IOPS comparison
plt.figure()
zeroq_sorted = zeroq.sort_values("jobname")
for _, row in zeroq_sorted.iterrows():
    label = row["jobname"]
    if pd.notna(row.get("read_iops")):
        plt.bar(label, row["read_iops"], color="blue")
    if pd.notna(row.get("write_iops")):
        plt.bar(label, row["write_iops"], color="orange")
plt.ylabel("IOPS")
plt.title("Zero-queue: IOPS comparison")
plt.xticks(rotation=30)
plt.tight_layout()
plt.savefig("plots/zeroq_iops.png", dpi=150)

# -------- 2. Block-size sweep --------
bs_df = df[df['jobname'].str.startswith("bs_")]
for pattern in ["randread","read"]:
    sub = bs_df[bs_df['rw']==pattern].copy()
    sub["bs_bytes"] = sub["bs"].str.replace("k","000").astype(int)
    sub = sub.sort_values("bs_bytes")

    # Bandwidth vs block size
    plt.figure()
    plt.plot(sub["bs_bytes"], sub["read_bw_kBps"]/1024, marker='o')
    plt.xscale("log", base=2)
    plt.xlabel("Block size (bytes)")
    plt.ylabel("Bandwidth (MB/s)")
    plt.title(f"{pattern} - Bandwidth vs Block size")
    plt.grid(True)
    plt.savefig(f"plots/bs_{pattern}_bandwidth.png", dpi=150)

    # Latency vs block size
    plt.figure()
    plt.plot(sub["bs_bytes"], sub["read_lat_mean_us"], marker='o', color='red')
    plt.xscale("log", base=2)
    plt.xlabel("Block size (bytes)")
    plt.ylabel("Latency (us)")
    plt.title(f"{pattern} - Latency vs Block size")
    plt.grid(True)
    plt.savefig(f"plots/bs_{pattern}_latency.png", dpi=150)

# -------- 3. Read/Write mix --------
mix_df = df[df['jobname'].str.startswith("mix_")].copy()
mix_df["mix"] = mix_df["jobname"].str.extract(r'mix_(\d+)R').astype(int)
mix_df = mix_df.sort_values("mix")

plt.figure()
plt.plot(mix_df["mix"], mix_df["read_iops"].fillna(mix_df["write_iops"]), marker='o')
plt.xlabel("% Reads")
plt.ylabel("IOPS")
plt.title("IOPS vs Read/Write Mix (4k randrw)")
plt.grid(True)
plt.savefig("plots/mix_iops.png", dpi=150)

plt.figure()
plt.plot(mix_df["mix"], mix_df["read_lat_mean_us"].fillna(mix_df["write_lat_mean_us"]), marker='o', color='red')
plt.xlabel("% Reads")
plt.ylabel("Latency (us)")
plt.title("Latency vs Read/Write Mix (4k randrw)")
plt.grid(True)
plt.savefig("plots/mix_latency.png", dpi=150)

plt.figure()
plt.plot(mix_df["mix"], (mix_df["read_bw_kBps"].fillna(mix_df["write_bw_kBps"])/1024), marker='o', color='green')
plt.xlabel("% Reads")
plt.ylabel("Bandwidth (MB/s)")
plt.title("Bandwidth vs Read/Write Mix (4k randrw)")
plt.grid(True)
plt.savefig("plots/mix_bandwidth.png", dpi=150)

# -------- 4. Queue depth sweep --------
qd_df = df[df['jobname'].str.startswith("qd_randread")].copy()
qd_df = qd_df.sort_values("iodepth")

# Latency vs QD
plt.figure()
plt.plot(qd_df["iodepth"], qd_df["read_lat_mean_us"], marker='o', color='red')
plt.xscale("log", base=2)
plt.xlabel("Queue Depth")
plt.ylabel("Latency (us)")
plt.title("Latency vs Queue Depth (4k randread)")
plt.grid(True)
plt.savefig("plots/qd_latency.png", dpi=150)

# Throughput vs QD
plt.figure()
plt.plot(qd_df["iodepth"], qd_df["read_iops"], marker='o', color='blue')
plt.xscale("log", base=2)
plt.xlabel("Queue Depth")
plt.ylabel("Throughput (IOPS)")
plt.title("Throughput vs Queue Depth (4k randread)")
plt.grid(True)
plt.savefig("plots/qd_throughput.png", dpi=150)

# Throughput vs Latency
plt.figure()
plt.plot(qd_df["read_lat_mean_us"], qd_df["read_iops"], marker='o')
for i, row in qd_df.iterrows():
    plt.annotate(f"QD={row['iodepth']}", (row["read_lat_mean_us"], row["read_iops"]))
plt.xlabel("Latency (us)")
plt.ylabel("Throughput (IOPS)")
plt.title("Throughput vs Latency (4k randread)")
plt.grid(True)
plt.savefig("plots/qd_tradeoff_curve.png", dpi=150)

# -------- 5. Tail latency --------
tail_df = df[df['jobname'].str.startswith("tail_lat")]
print("\nTail latency characterization:")
print(tail_df[["jobname","iodepth","read_p50_us","read_p95_us","read_p99_us","read_p99_us"]])
