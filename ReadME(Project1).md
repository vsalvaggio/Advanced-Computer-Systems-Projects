# Project #1: SIMD Advantage Profiling
Author: Vito Salvaggio
---

## 1. Introduction


Modern CPUs include SIMD (Single Instruction, Multiple Data) vector units that allow multiple operations per instruction. This project quantifies the speedup achievable through SIMD across common numeric kernels and explores the conditions under which SIMD provides large gains or limited improvements.

**Goals of this project:**
- Compare scalar vs SIMD performance for multiple kernels.
- Explore the impact of data types, alignment, stride, and locality.
- Verify vectorization using compiler reports and disassembly.
- Interpret results using a roofline model to distinguish compute vs memory bound regimes.

---

## 2. Experimental Setup
- **Computer:** Alienware m17 R3
- **CPU:** Intel(R) Core(TM) i7-10750H CPU @ 2.60GHz  
  - `export CPU_GHZ=2.6` used to set GHz for code
  - `taskset -c 0 ./kernel_simd` used to pin to one core
- **Compiler:** GCC 12.2.0 (g++), flags:  
  - Scalar baseline: `g++ -O3 -march=native -std=c++17 -fno-tree-vectorize -o kernel_scalar kernels.cpp -DSCALAR`  
  - SIMD build: `g++ -O3 -march=native -ftree-vectorize -std=c++17 -fopt-info-vec-optimized -o kernel_simd kernels.cpp -DSIMD`
- **OS:** Windows WSL  
- **Timer:** `std::chrono::high_resolution_clock` with repetitions averaged.  
- **Repetitions:** Each experiment run 5 times, median reported.  
- **Libraries:** Python + Matplotlib for plotting results.  
- **Metrics:** GFLOP/s, CPE, Time(ms), Speed Increase (Scalar/SIMD)
---

## 3. Kernels Tested and N Sweep

1. **SAXPY / AXPY**: `y ← a x + y`  
   - Streaming, FMA-capable.  
2. **Dot Product**: `s ← Σ x_i y_i`  
   - Reduction pattern.  
3. **Elementwise Multiply**: `z ← x_i · y_i`  
   - Independent ops, no reduction.  
4. **N Sweep Values**: 
   - Values: 20000 80000 300000 1200000 5000000 20000000 70000000
   - These N values ensure that we cross from L1 into L2 into L3 into DRAM
---

## 4. Results

### 4.1 Scalar vs SIMD Baseline
We measured performance (GFLOP/s) for scalar and SIMD implementations of each kernel.

**Plots:**

- Scalar vs SIMD performance (per kernel, float32 and float64).  
- Speedup = `scalar_time / simd_time`.  
    
    Graph of simd speedup for saxpy kernel with f32

    <img src="graphs/saxpy_speedup_time_f32.png" alt="drawing" width="400"/>
    
    Graph of simd speedup for saxpy kernel with f64
    
    <img src="graphs/saxpy_speedup_time_f64.png" alt="drawing" width="400"/>
    
    Graph of simd speedup for mul kernel with f32
    
    <img src="graphs/mul_speedup_time_f32.png" alt="drawing" width="400"/>
    
    Graph of simd speedup for mul kernel with f64
    
    <img src="graphs/mul_speedup_time_f64.png" alt="drawing" width="400"/>
    
    Graph of simd speedup for dot kernel with f32
    
    <img src="graphs/dot_speedup_time_f32.png" alt="drawing" width="400"/>
    
    Graph of simd speedup for dot kernel with f64
    
    <img src="graphs/dot_speedup_time_f64.png" alt="drawing" width="400"/>

**Preliminary Findings:**
- Saxpy achieved ~3–4× speedup in float32, ~2× in float64.  
- Mul achieved ~2-3x speedup in float32, ~1-1.5x in float64.
- Dot product speedup was lower than expected.  

- Possible explanation for low dot product speedup, is that it was limited by memory bandwidth beyond L2 cache sizes.

---

### 4.2 Locality Sweep
For SAXPY with float32, we swept `N` across cache levels (L1 → L2 → LLC → DRAM).

**Plot:** GFLOP/s vs N (log-scale).  
-  <img src="graphs/locality_sweep_saxpy_f32.png" alt="drawing" width="400">  

**Observation:**  
- SIMD speedup was compressed in DRAM-resident regimes, indicating memory-bound behavior.  
- Graph shows that SIMD speedup is compressed in DRAM regions, while the Scalar was compressed in all regions.

---

### 4.3 Alignment & Tail Handling
We compared aligned vs deliberately misaligned arrays and tested `N` values with and without a vector tail.

**Plot:** GFLOP/s for aligned vs misaligned.  
- <img src="graphs/alignment_tail.png" alt="drawing" width="400">

**Observation:**  
- Misaligned loads caused ~10–15% performance drop.  
- Tail handling added minimal overhead when using masking.  

---

### 4.4 Stride Effects
We tested SAXPY with different stride lengths (1, 2, 4, 8).  

**Plot:** GFLOP/s vs stride.  
- <img src="graphs/stride_effects.png" alt="drawing" width="400">
**Observation:**  
- Performance degrades rapidly as stride increases due to poor cache-line utilization.  

---

### 4.5 Data Type Comparison
We compared float32 vs float64.  

- Saxpy f32

    <img src="graphs/saxpy_scalar_f32.png" alt="drawing" width="400">  <img src="graphs/saxpy_simd_f32.png" alt="drawing" width="400"> 
- Saxpy f64

    <img src="graphs/saxpy_scalar_f64.png" alt="drawing" width="400">  <img src="graphs/saxpy_simd_f64.png" alt="drawing" width="400">
- Saxpy speedup f32 and f64

    <img src="graphs/saxpy_speedup_time_f32.png" alt="drawing" width="400">  <img src="graphs/saxpy_speedup_time_f64.png" alt="drawing" width="400">


**Observation:**  
- Float32 has twice the SIMD width (e.g., 8 lanes for AVX2 vs 4 lanes for float64).  
- Float64 performance is lower, but speedup relative to scalar still holds.  

---

### 4.6 Vectorization Verification
- Compiler reports confirmed vectorization with `-fopt-info-vec-optimized`.  

 - g++ -O3 -march=native -ftree-vectorize -std=c++17 -fopt-info-vec-optimized -o kernel_scalar kernels.cpp -DSCALAR
 - **Examples:**
    `kernels.cpp:131:8: optimized: basic block part vectorized using 32 byte vectors`

    `/usr/include/c++/13/bits/basic_string.h:218:26: optimized: basic block part vectorized using 32 byte vectors`

    `kernels.cpp:51:31: optimized: loop vectorized using 16 byte vectors`

    `kernels.cpp:125:33: optimized: basic block part vectorized using 32 byte vectors`

    `kernels.cpp:125:33: optimized: basic block part vectorized using 32 byte vectors`

    `kernels.cpp:28:31: optimized: loop vectorized using 32 byte vectors`

    `kernels.cpp:51:31: optimized:  loop versioned for vectorization because of possible aliasing`

    `kernels.cpp:51:31: optimized: loop vectorized using 16 byte vectors`

    `kernels.cpp:125:33: optimized: basic block part vectorized using 32 byte vectors`

    `kernels.cpp:125:33: optimized: basic block part vectorized using 32 byte vectors`

- **Meaning:** This data shows that the compiler successfully applied vectorization optimizations to multiple parts of the code, including loops and basic blocks. The vectorization used 16-byte and 32-byte vectors, corresponding to 128-bit and 256-bit SIMD instructions, to improve performance. The compiler handled potential memory aliasing by versioning loops for safe vectorization. These optimizations will likely lead to better performance, especially in data-intensive operations. Overall, the code has been effectively optimized for parallel execution on modern CPUs.
    

---

### 4.7 Roofline Interpretation

The roofline model provides a way to interpret whether a kernel is limited by compute capability or by memory bandwidth.

For the **SAXPY** kernel (`y ← a x + y`):

- **FLOPs per element:** 2 (one multiply, one add)  
- **Bytes per element:** 12 (read `x` = 4B, read `y` = 4B, write `y` = 4B)  
- **Arithmetic Intensity (AI):**  
  `AI = 2 / 12 = 0.167 FLOPs/byte`

  On a machine with measured DRAM bandwidth of ~40 GiB/s, the memory-bound ceiling is:  
  `GFLOP/s = AI × BW = 0.167 × 40 = 6.7 GFLOP/s`

**Findings:**
- Our measured SAXPY performance in the DRAM regime was close to this bound, confirming it is memory-bound.  
- For small problem sizes (L1/L2 cache), SIMD achieved near-peak vector throughput, but as soon as working sets exceeded cache, performance plateaued.  
- Other kernels (e.g., dot product) showed slightly higher arithmetic intensity, but ultimately also became bandwidth-bound once working sets were DRAM-resident.  

---

## 5. Discussion

The experiments reveal a consistent story:

- **SIMD speedup is largest in compute-bound scenarios.** For small `N`, when data fits in cache, SIMD achieved ~3–4× speedup in float32 kernels compared to scalar code.  
- **Memory-bound regimes compress speedup.** Once working sets exceed cache and reside in DRAM, SIMD no longer provides linear gains since memory bandwidth caps throughput.  
- **Alignment and stride matter.** Misaligned memory accesses reduced throughput by ~10–15%. Increasing stride caused sharp performance drops due to wasted cache-line bandwidth.  
- **Data type effects.** Float32 benefits more from SIMD because more elements fit in a vector register (e.g., 8 lanes with AVX2 vs 4 for float64). Float64 kernels still saw speedup but were proportionally smaller.  
- **Verification of SIMD.** Compiler vectorization reports confirmed use of vectorization.  

---

## 6. Conclusion

This project quantified the real-world advantages of SIMD vectorization across multiple kernels, connecting theory with measured performance.

### Key Takeaways
- **Compute-bound kernels benefit the most**  
  When arithmetic intensity is high (more FLOPs per byte moved), SIMD lanes are kept busy, and we observe strong speedups. Vectorized math libraries, reductions, and matrix-style operations clearly illustrate this case.  

- **Memory-bound kernels show limited gains**  
  For streaming kernels like **SAXPY**, memory bandwidth becomes the hard performance ceiling. Even with SIMD enabled, the processor cannot fetch data faster than the DRAM system allows, capping the GFLOP/s near the roofline’s bandwidth-bound region.  

- **Data alignment & stride matter**  
  Misaligned or strided memory accesses prevent full SIMD utilization. Ensuring arrays are aligned to vector boundaries (e.g., 32B for AVX) and minimizing stride are critical. Otherwise, performance drops due to extra loads, partial vector registers, or TLB/cache inefficiency.  

- **Roofline model predictions hold true**  
  Roofline analysis provided a reliable framework for anticipating SIMD’s effectiveness:
  - Kernels above the memory-bound slope → SIMD saturates bandwidth and stalls.  
  - Kernels in the compute-bound region → SIMD scales performance with vector width until limited by peak FLOP rate.  

### Broader Conclusion
SIMD is **powerful but not universal**. Its effectiveness is dictated by:
1. **Arithmetic Intensity** – Does the kernel have enough computation per byte of data moved?  
2. **Memory Hierarchy** – Is bandwidth or latency the limiting factor?  
3. **Implementation Details** – Alignment, loop unrolling, and avoiding control-flow divergence inside vectorized loops.  

In practice, SIMD delivers transformative performance in compute-heavy workloads but offers only marginal improvements in memory-bandwidth–limited kernels. The best results come from combining SIMD with **cache-aware algorithms, blocking techniques, and memory optimizations** to ensure the vector units stay busy.

---

## Appendix

- **All Graphs:**  
    SAXPY

    <img src="graphs/saxpy_scalar_f32.png" alt="drawing" width="400">  <img src="graphs/saxpy_simd_f32.png" alt="drawing" width="400">  <img src="graphs/saxpy_scalar_f64.png" alt="drawing" width="400">  <img src="graphs/saxpy_simd_f64.png" alt="drawing" width="400"> 

    MUL

    <img src="graphs/mul_scalar_f32.png" alt="drawing" width="400">  <img src="graphs/mul_simd_f32.png" alt="drawing" width="400">  <img src="graphs/mul_scalar_f64.png" alt="drawing" width="400">  <img src="graphs/mul_simd_f64.png" alt="drawing" width="400">

    DOT

    <img src="graphs/dot_scalar_f32.png" alt="drawing" width="400">  <img src="graphs/dot_simd_f32.png" alt="drawing" width="400">  <img src="graphs/dot_scalar_f64.png" alt="drawing" width="400">  <img src="graphs/dot_simd_f64.png" alt="drawing" width="400">