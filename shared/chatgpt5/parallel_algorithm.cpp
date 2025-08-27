// mandelbrot_pstl_pbm_timed.cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <execution>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>
#include <cstdlib>

#if __has_include(<tbb/global_control.h>)
  #include <tbb/global_control.h>
  #define HAVE_TBB 1
#else
  #define HAVE_TBB 0
#endif

static inline uint8_t in_set(double cr, double ci, int max_iter) {
    double zr = 0.0, zi = 0.0;
    int i = 0;
    while (zr*zr + zi*zi <= 4.0 && i < max_iter) {
        double zr_new = zr*zr - zi*zi + cr;
        zi = 2.0*zr*zi + ci;
        zr = zr_new;
        ++i;
    }
    return (i == max_iter) ? 1u : 0u; // PBM: 1 = black (inside), 0 = white (outside)
}

static void write_pbm(const std::string& path,
                      const std::vector<uint8_t>& mask,
                      int width, int height)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) { std::cerr << "Failed to open " << path << "\n"; std::exit(1); }
    out << "P4\n" << width << " " << height << "\n";

    const int row_bytes = (width + 7) / 8;
    std::vector<unsigned char> rowbuf(static_cast<size_t>(row_bytes));
    for (int y = 0; y < height; ++y) {
        std::fill(rowbuf.begin(), rowbuf.end(), 0);
        const uint8_t* row = &mask[static_cast<size_t>(y) * width];
        for (int x = 0; x < width; ++x) {
            if (row[x]) {
                const int byte_index = x / 8;
                const int bit_index  = 7 - (x % 8); // MSB first
                rowbuf[byte_index] |= static_cast<unsigned char>(1u << bit_index);
            }
        }
        out.write(reinterpret_cast<const char*>(rowbuf.data()), row_bytes);
    }
}

static void usage(const char* prog) {
    std::cerr <<
      "Usage: " << prog << " [-t threads] [width height [max_iter [x_center y_center [scale]]]]\n"
      "Defaults: 1920 1080 1000 -0.75 0.0 3.0; threads=hardware_concurrency\n";
}

int main(int argc, char** argv) {
    int    width     = 1920;
    int    height    = 1080;
    int    max_iter  = 1000;
    double x_center  = -0.75;
    double y_center  = 0.0;
    double scale     = 3.0; // view width in complex plane
    int    threads   = static_cast<int>(std::thread::hardware_concurrency());

    // Parse args
    std::vector<std::string> pos;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s == "-t") {
            if (i + 1 >= argc) { usage(argv[0]); return 1; }
            int t = std::atoi(argv[++i]);
            if (t > 0) threads = t;
        } else if (s == "-h" || s == "--help") {
            usage(argv[0]); return 0;
        } else {
            pos.push_back(std::move(s));
        }
    }
    if (!pos.empty()) {
        if (pos.size() >= 1) width  = std::max(16, std::atoi(pos[0].c_str()));
        if (pos.size() >= 2) height = std::max(16, std::atoi(pos[1].c_str()));
        if (pos.size() >= 3) max_iter = std::max(10, std::atoi(pos[2].c_str()));
        if (pos.size() >= 5) { x_center = std::atof(pos[3].c_str()); y_center = std::atof(pos[4].c_str()); }
        if (pos.size() >= 6) scale = std::atof(pos[5].c_str());
    }

    // Map image to complex plane
    const double aspect = static_cast<double>(width) / height;
    const double x_min  = x_center - 0.5 * scale;
    const double x_max  = x_center + 0.5 * scale;
    const double y_min  = y_center - 0.5 * scale / aspect;
    const double y_max  = y_center + 0.5 * scale / aspect;
    const double dx     = (x_max - x_min) / (width  - 1);
    const double dy     = (y_max - y_min) / (height - 1);

    std::vector<uint8_t> mask(static_cast<size_t>(width) * height, 0);

    // Build a row list; each row runs in parallel, inner loop is sequential & cache-friendly
    std::vector<int> rows(height);
    std::iota(rows.begin(), rows.end(), 0);

#if HAVE_TBB
    // Limit PSTL backend threads (libstdc++ PSTL commonly uses oneTBB)
    tbb::global_control limit(tbb::global_control::max_allowed_parallelism,
                              std::max(1, threads));
#else
    // Some implementations may honor these env vars; harmless otherwise.
    // (Set before parallel region; effect is implementation-defined.)
    setenv("TBB_NUM_THREADS", std::to_string(std::max(1, threads)).c_str(), 1);
    setenv("OMP_NUM_THREADS", std::to_string(std::max(1, threads)).c_str(), 1);
#endif

    // ---- compute (EXCLUDES I/O) ----
    auto t0 = std::chrono::steady_clock::now();

    std::for_each(std::execution::par, rows.begin(), rows.end(),
                  [&](int y) {
                      uint8_t* row = &mask[static_cast<size_t>(y) * width];
                      const double ci = y_max - y * dy; // image Y downward
                      for (int x = 0; x < width; ++x) {
                          const double cr = x_min + x * dx;
                          row[x] = in_set(cr, ci, max_iter);
                      }
                  });

    auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "Compute time (no I/O): " << ms << " ms with " << threads << " threads\n";

    // ---- I/O (not timed) ----
    write_pbm("mandelbrot.pbm", mask, width, height);
    std::cout << "Wrote mandelbrot.pbm (" << width << "x" << height
              << ", max_iter=" << max_iter << ")\n";
    return 0;
}

