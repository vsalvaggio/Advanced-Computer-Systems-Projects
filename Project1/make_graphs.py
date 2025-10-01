import os
import pandas as pd
import matplotlib.pyplot as plt

# =============== Ensure 'results' directory exists ===============
if not os.path.exists("graphs"):
    os.makedirs("graphs")
# =============== Load Data ===============
df_loc    = pd.read_csv("C:/Users/vsalv/Desktop/C++Files/Advanced Computer Systems/results/output.csv")
df_align  = pd.read_csv("C:/Users/vsalv/Desktop/C++Files/Advanced Computer Systems/results/misalign_output.csv")
df_stride = pd.read_csv("C:/Users/vsalv/Desktop/C++Files/Advanced Computer Systems/results/stride_output.csv")

# =============== 1. Per-Kernel Breakdown ===============
for k in df_loc['kernel'].unique():
    for dtype in ['f32', 'f64']:
        for impl in ['simd', 'scalar']:
            subset = df_loc[(df_loc['kernel']==k) &
                            (df_loc['dtype']==dtype) &
                            (df_loc['impl']==impl)]

            if subset.empty:
                continue

            plt.figure(figsize=(8,6))
            plt.plot(subset['N'], subset['gflops'], marker='o', label=f"{impl}-{dtype}")
            plt.xscale("log")
            plt.xlabel("Problem Size N")
            plt.ylabel("GFLOP/s")
            plt.title(f"{k.upper()} – {impl.upper()} {dtype}")
            plt.grid(True, linestyle="--", alpha=0.6)
            plt.legend()
            plt.savefig(f"graphs/{k}_{impl}_{dtype}.png")
            plt.close()

# =============== 2. Speedup Plots ===============
for k in df_loc['kernel'].unique():
    for dtype in ['f32','f64']:
        simd = df_loc[(df_loc['kernel']==k)&(df_loc['dtype']==dtype)&(df_loc['impl']=='simd')]
        scalar = df_loc[(df_loc['kernel']==k)&(df_loc['dtype']==dtype)&(df_loc['impl']=='scalar')]

        if simd.empty or scalar.empty:
            continue

        # align on N
        merged = pd.merge(simd[['N','time_ms']], scalar[['N','time_ms']],
                          on='N', suffixes=('_simd','_scalar'))
        
        # Speedup = scalar time / simd time
        merged['speedup'] = merged['time_ms_scalar'] / merged['time_ms_simd']

        plt.figure(figsize=(8,6))
        plt.plot(merged['N'], merged['speedup'], marker='o', color='purple')
        plt.xscale("log")
        plt.xlabel("Problem Size N")
        plt.ylabel("Speedup (Scalar Time / SIMD Time)")
        plt.title(f"{k.upper()} Speedup (Time-based) – {dtype}")
        plt.grid(True, linestyle="--", alpha=0.6)
        plt.savefig(f"graphs/{k}_speedup_time_{dtype}.png")
        plt.close()
# =============== 3. Locality Sweep (Single Kernel, f32) ===============
one_kernel = df_loc[df_loc['kernel'] == 'saxpy']   # pick saxpy (or change)
subset = one_kernel[(one_kernel['dtype']=='f32')]

plt.figure(figsize=(8,6))
for impl in subset['impl'].unique():
    impl_data = subset[subset['impl']==impl]
    plt.plot(impl_data['N'], impl_data['gflops'], marker='o', label=impl)
plt.xscale("log")
plt.xlabel("Problem Size N")
plt.ylabel("GFLOP/s")
plt.title("Locality Sweep (SAXPY f32)")
plt.grid(True, linestyle="--", alpha=0.6)
plt.legend()
plt.savefig("graphs/locality_sweep_saxpy_f32.png")
plt.close()

# =============== 4. Stride Effects ===============
df_stride['stride'] = df_stride['stride'].astype(int)
plt.figure(figsize=(8,6))
plt.plot(df_stride['stride'], df_stride['gflops'], marker='o')
plt.xlabel("Stride")
plt.ylabel("GFLOP/s")
plt.title("Stride Effects (mul, f32, N=20M)")
plt.grid(True, linestyle="--", alpha=0.6)
plt.savefig("graphs/stride_effects.png")
plt.close()

# =============== 5. Alignment & Tail Handling ===============
plt.figure(figsize=(8,6))
for mis in df_align['misalign'].unique():
    subset = df_align[df_align['misalign']==mis]
    plt.bar([f"{r['dtype']}-{r['N']}" for _, r in subset.iterrows()],
            subset['gflops'], label=f"misalign={mis}")
plt.ylabel("GFLOP/s")
plt.title("Alignment & Tail Handling (SAXPY)")
plt.legend()
plt.xticks(rotation=45)
plt.tight_layout()
plt.savefig("graphs/alignment_tail.png")
plt.close()

print("✅ All graphs generated in graphs/ folder")
