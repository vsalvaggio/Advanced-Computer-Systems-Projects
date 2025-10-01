#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <filesystem>
#include <cmath>
#include <limits>
#include <cstdlib>

// =======================
// FLOPs per element
// =======================
inline double saxpy_flops() { return 2.0; }   // 1 mul + 1 add
inline double dot_flops()   { return 2.0; }   // 1 mul + 1 add
inline double mul_flops()   { return 1.0; }   // 1 mul

// =======================
// Kernels
// =======================

// y = a*x + y
template<typename T>
void saxpy_kernel(std::size_t n, T a, const T* x, T* y, std::size_t stride = 1) {
    for (std::size_t i = 0; i < n; i++) {
        y[i * stride] = a * x[i * stride] + y[i * stride];
    }
}

// global volatile sink to prevent compiler from optimizing away results
template <typename T>
volatile T dot_sink;

// dot = sum(x*y)
template<typename T>
T dot_kernel(std::size_t n, const T* x, const T* y, std::size_t stride = 1) {
    T result = T(0);
    for (std::size_t i = 0; i < n; i++) {
        result += x[i * stride] * y[i * stride];
    }
    dot_sink<T> = result;  // store result so it cannot be optimized away
    return result;
}

// z = x*y
template<typename T>
void mul_kernel(std::size_t n, const T* x, const T* y, T* z, std::size_t stride = 1) {
    for (std::size_t i = 0; i < n; i++) {
        z[i * stride] = x[i * stride] * y[i * stride];
    }
}

// =======================
// Utility: Aligned buffers
// =======================
template<typename T>
struct BufferSet {
    T* x;
    T* y;
    T* z;
    void* raw;
};

template<typename T>
BufferSet<T> make_buffers(std::size_t n, std::size_t align, std::size_t misalign) {
    void* mem = nullptr;
    std::size_t bytes = sizeof(T) * (3 * n) + align + misalign;
    if (posix_memalign(&mem, align, bytes)) {
        throw std::bad_alloc();
    }
    char* base = reinterpret_cast<char*>(mem) + misalign;
    T* px = reinterpret_cast<T*>(base);
    T* py = px + n;
    T* pz = py + n;
    return {px, py, pz, mem};
}

template<typename T>
void fill_buffers(BufferSet<T>& B, std::size_t n, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(1.0, 2.0);
    for (std::size_t i = 0; i < n; i++) {
        B.x[i] = static_cast<T>(dist(rng));
        B.y[i] = static_cast<T>(dist(rng));
        B.z[i] = T(0);
    }
}

// =======================
// Benchmarking helpers
// =======================
struct Result {
    double ms;
    double gflops;
    double cpe;
};

static double cpu_ghz_from_env() {
    const char* v = std::getenv("CPU_GHZ");
    if (!v) return 0.0;
    return std::atof(v);
}

template<typename F>
Result run_benchmark(std::size_t n, std::size_t reps, double flops_per_elem, F&& func) {
    using clk = std::chrono::high_resolution_clock;
    double best_ms = 1e300;
    for (std::size_t r = 0; r < reps; r++) {
        auto t0 = clk::now();
        func();
        auto t1 = clk::now();
        double elapsed = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (elapsed < best_ms) best_ms = elapsed;
    }
    double gflops = (n * flops_per_elem) / (best_ms * 1e-3) / 1e9;
    double ghz = cpu_ghz_from_env();
    double cpe = std::numeric_limits<double>::quiet_NaN();
    if (ghz > 0) {
        double cycles = best_ms * 1e-3 * ghz * 1e9;
        cpe = cycles / double(n);
    }
    return {best_ms, gflops, cpe};
}

// =======================
// Command-line parsing
// =======================
struct Options {
    std::string kernel = "saxpy";   // saxpy, dot, mul
    std::string dtype = "f32";      // f32 or f64
    std::string impl = "scalar";    // scalar or simd
    std::size_t N = 1 << 20;
    std::size_t reps = 3;
    std::size_t stride = 1;
    std::size_t align = 64;
    std::size_t misalign = 0;
    unsigned seed = 32517;
    std::string csv = "results/output.csv";
};

Options parse_args(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto need = [&](const char* flag) {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << flag << "\n";
                std::exit(1);
            }
            return std::string(argv[++i]);
        };
        if (a == "--kernel") opt.kernel = need("--kernel");
        else if (a == "--dtype") opt.dtype = need("--dtype");
        else if (a == "--impl") opt.impl = need("--impl");
        else if (a == "--N") opt.N = std::stoull(need("--N"));
        else if (a == "--reps") opt.reps = std::stoull(need("--reps"));
        else if (a == "--stride") opt.stride = std::stoull(need("--stride"));
        else if (a == "--align") opt.align = std::stoull(need("--align"));
        else if (a == "--misalign") opt.misalign = std::stoull(need("--misalign"));
        else if (a == "--csv") opt.csv = need("--csv");
        else {
            std::cerr << "Unknown argument: " << a << "\n";
            std::exit(1);
        }
    }
    return opt;
}

// =======================
// CSV output
// =======================
static void ensure_parent_dir(const std::string& path) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
}

static void write_csv_header(const std::string& path) {
    if (std::filesystem::exists(path)) return;
    ensure_parent_dir(path);
    std::ofstream f(path);
    f << "time,kernel,dtype,impl,N,stride,misalign,time_ms,gflops,cpe\n";
}

static void append_csv(const std::string& path,
                       const std::string& kernel,
                       const std::string& dtype,
                       const std::string& impl,
                       std::size_t N,
                       std::size_t stride,
                       std::size_t misalign,
                       const Result& R) {
    std::ofstream f(path, std::ios::app);
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    f << std::put_time(std::localtime(&now), "%F %T") << ","
      << kernel << "," << dtype << "," << impl << "," << N << "," << stride << ","
      << misalign << ","
      << std::fixed << std::setprecision(6)
      << R.ms << "," << R.gflops << "," << R.cpe << "\n";
}

// =======================
// Main driver
// =======================
template<typename T>
int run_kernel(const Options& opt) {
    auto B = make_buffers<T>(opt.N * opt.stride + 8, opt.align, opt.misalign);
    fill_buffers(B, opt.N * opt.stride + 8, opt.seed);
    T a = T(1.111);

    double flops_elem = 0.0;
    if (opt.kernel == "saxpy") flops_elem = saxpy_flops();
    else if (opt.kernel == "dot") flops_elem = dot_flops();
    else if (opt.kernel == "mul") flops_elem = mul_flops();
    else {
        std::cerr << "Unknown kernel\n";
        return 1;
    }

    Result R = run_benchmark(opt.N, opt.reps, flops_elem, [&]() {
        if (opt.kernel == "saxpy") saxpy_kernel<T>(opt.N, a, B.x, B.y, opt.stride);
        else if (opt.kernel == "dot") (void)dot_kernel<T>(opt.N, B.x, B.y, opt.stride);
        else if (opt.kernel == "mul") mul_kernel<T>(opt.N, B.x, B.y, B.z, opt.stride);
    });

    write_csv_header(opt.csv);
    append_csv(opt.csv, opt.kernel, (sizeof(T) == 4 ? "f32" : "f64"),
               opt.impl ,opt.N, opt.stride, opt.misalign, R);

    free(B.raw);
    return 0;
}

int main(int argc, char** argv) {
    Options opt = parse_args(argc, argv);
    if (opt.dtype == "f32") return run_kernel<float>(opt);
    else return run_kernel<double>(opt);
}