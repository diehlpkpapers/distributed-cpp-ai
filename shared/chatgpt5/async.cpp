// mandelbrot_async_pbm.cpp
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <future>
#include <algorithm>
#include <thread>

// ---------- Mandelbrot membership (1=inside/black, 0=outside/white) ----------
static inline unsigned char in_set(double cr, double ci, int max_iter) {
    double zr = 0.0, zi = 0.0;
    int i = 0;
    while (zr*zr + zi*zi <= 4.0 && i < max_iter) {
        double zr_new = zr*zr - zi*zi + cr;
        zi = 2.0*zr*zi + ci;
        zr = zr_new;
        ++i;
    }
    return (i == max_iter) ? 1u : 0u;
}

// ---------- PBM (P4) writer: 1 bit/pixel, MSB first ----------
static void write_pbm(const std::string& path,
                      const std::vector<unsigned char>& mask,
                      int width, int height)
{
    std::ofstream out(path.c_str(), std::ios::binary);
    if (!out) { std::cerr << "Error: cannot open " << path << "\n"; std::exit(1); }
    out << "P4\n" << width << " " << height << "\n";

    const int row_bytes = (width + 7) / 8;
    std::vector<unsigned char> rowbuf(static_cast<size_t>(row_bytes));
    for (int y = 0; y < height; ++y) {
        std::fill(rowbuf.begin(), rowbuf.end(), 0);
        const unsigned char* row = &mask[static_cast<size_t>(y) * width];
        for (int x = 0; x < width; ++x) {
            if (row[x]) {
                const int bi = x / 8;
                const int bit = 7 - (x % 8);
                rowbuf[bi] |= static_cast<unsigned char>(1u << bit);
            }
        }
        out.write(reinterpret_cast<const char*>(rowbuf.data()), row_bytes);
    }
}

// ---------- Main ----------
int main(int argc, char** argv) {
    // Defaults
    int width = 192000, height = 10800, max_iter = 1000;
    double x_center = -0.75, y_center = 0.0, scale = 3.0;
    int threads = static_cast<int>(std::thread::hardware_concurrency());
    if (threads <= 0) threads = 1;

    // Parse: [-t N] [width height [max_iter [x_center y_center [scale]]]]
    std::vector<std::string> pos;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s == "-t") {
            if (i + 1 >= argc) { std::cerr << "Usage: " << argv[0] << " -t N [...]\n"; return 1; }
            int t = std::atoi(argv[++i]);
            if (t > 0) threads = t;
        } else if (s == "-h" || s == "--help") {
            std::cerr <<
              "Usage: " << argv[0] << " [-t threads] [width height [max_iter [x_center y_center [scale]]]]\n";
            return 0;
        } else {
            pos.push_back(s);
        }
    }
    if (!pos.empty()) {
        if (pos.size() >= 1) width  = std::max(16, std::atoi(pos[0].c_str()));
        if (pos.size() >= 2) height = std::max(16, std::atoi(pos[1].c_str()));
        if (pos.size() >= 3) max_iter = std::max(10, std::atoi(pos[2].c_str()));
        if (pos.size() >= 5) { x_center = std::atof(pos[3].c_str()); y_center = std::atof(pos[4].c_str()); }
        if (pos.size() >= 6) scale = std::atof(pos[5].c_str());
    }

    // Cap threads to image height (one chunk per worker)
    threads = std::min(threads, std::max(1, height));

    // Precompute mapping
    const double aspect = static_cast<double>(width) / height;
    const double x_min  = x_center - 0.5 * scale;
    const double x_max  = x_center + 0.5 * scale;
    const double y_min  = y_center - 0.5 * scale / aspect;
    const double y_max  = y_center + 0.5 * scale / aspect;
    const double dx     = (x_max - x_min) / (width  - 1);
    const double dy     = (y_max - y_min) / (height - 1);

    std::vector<unsigned char> mask(static_cast<size_t>(width) * height, 0);

    // Partition rows into 'threads' chunks; each task handles a range
    const int rows_per = (height + threads - 1) / threads;

    // ---- Compute timing (EXCLUDES I/O) ----
    const auto t0 = std::chrono::steady_clock::now();

    std::vector<std::future<void> > futs;
    futs.reserve(static_cast<size_t>(threads));
    for (int w = 0; w < threads; ++w) {
        const int y0 = w * rows_per;
        const int y1 = std::min(height, y0 + rows_per);
        if (y0 >= y1) break;

        futs.push_back(std::async(std::launch::async,
            [=, &mask]() {
                for (int y = y0; y < y1; ++y) {
                    const double ci = y_max - y * dy; // image Y downward
                    unsigned char* row = &mask[static_cast<size_t>(y) * width];
                    for (int x = 0; x < width; ++x) {
                        const double cr = x_min + x * dx;
                        row[x] = in_set(cr, ci, max_iter);
                    }
                }
            }
        ));
    }

    for (auto& f : futs) f.get(); // propagate exceptions & wait

    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "Compute time (no I/O): " << ms << " ms with " << threads << " threads\n";

    // ---- I/O (NOT timed) ----
    write_pbm("mandelbrot.pbm", mask, width, height);
    std::cout << "Wrote mandelbrot.pbm (" << width << "x" << height
              << ", max_iter=" << max_iter << ")\n";

    return 0;
}

