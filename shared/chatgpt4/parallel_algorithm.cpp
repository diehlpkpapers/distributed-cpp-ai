#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>
#include <execution>
#include <chrono>
#include <string>
#include <cstdlib>
#include <thread>
#include <numeric>

// ------------------ Mandelbrot Evaluation ------------------
uint8_t mandelbrot(double cr, double ci, int max_iter) {
    double zr = 0.0, zi = 0.0;
    int i = 0;
    while (zr * zr + zi * zi <= 4.0 && i < max_iter) {
        double temp = zr * zr - zi * zi + cr;
        zi = 2.0 * zr * zi + ci;
        zr = temp;
        ++i;
    }
    return (i == max_iter) ? 1 : 0;
}

// ------------------ PBM Writer ------------------
void write_pbm(const std::string& filename, const std::vector<uint8_t>& mask, int width, int height) {
    std::ofstream out(filename, std::ios::binary);
    out << "P4\n" << width << " " << height << "\n";
    int row_bytes = (width + 7) / 8;
    std::vector<unsigned char> rowbuf(row_bytes);

    for (int y = 0; y < height; ++y) {
        std::fill(rowbuf.begin(), rowbuf.end(), 0);
        for (int x = 0; x < width; ++x) {
            if (mask[y * width + x]) {
                rowbuf[x / 8] |= (1 << (7 - (x % 8)));
            }
        }
        out.write(reinterpret_cast<const char*>(rowbuf.data()), row_bytes);
    }
}

// ------------------ Main ------------------
int main(int argc, char** argv) {
    int width = 1920;
    int height = 1080;
    int max_iter = 1000;
    int threads = std::thread::hardware_concurrency();
    double x_center = -0.75;
    double y_center = 0.0;
    double scale = 3.0;

    // CLI: [-t threads] width height max_iter x_center y_center scale
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-t" && i + 1 < argc) {
            threads = std::atoi(argv[++i]);
        }
    }

    if (argc >= 3) width  = std::max(16, std::atoi(argv[1]));
    if (argc >= 4) height = std::max(16, std::atoi(argv[2]));
    if (argc >= 5) max_iter = std::atoi(argv[3]);
    if (argc >= 7) {
        x_center = std::atof(argv[4]);
        y_center = std::atof(argv[5]);
    }
    if (argc >= 8) scale = std::atof(argv[6]);

    std::cout << "Threads: " << threads << ", Size: " << width << "x" << height << ", Max iter: " << max_iter << "\n";

    // Set thread count via environment variable (honored by TBB and libparallel)
    std::string env = "OMP_NUM_THREADS=" + std::to_string(threads);
    putenv(&env[0]);

    const double aspect = static_cast<double>(width) / height;
    const double x_min = x_center - 0.5 * scale;
    const double x_max = x_center + 0.5 * scale;
    const double y_min = y_center - 0.5 * scale / aspect;
    const double y_max = y_center + 0.5 * scale / aspect;

    const double dx = (x_max - x_min) / (width - 1);
    const double dy = (y_max - y_min) / (height - 1);

    std::vector<uint8_t> mask(width * height);
    std::vector<size_t> indices(width * height);
    std::iota(indices.begin(), indices.end(), 0);

    // ---------- Compute timer starts ----------
    auto t0 = std::chrono::steady_clock::now();

    std::for_each(std::execution::par, indices.begin(), indices.end(), [&](size_t i) {
        int y = i / width;
        int x = i % width;
        double cr = x_min + x * dx;
        double ci = y_max - y * dy;
        mask[i] = mandelbrot(cr, ci, max_iter);
    });

    auto t1 = std::chrono::steady_clock::now();
    double runtime_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    // ---------- Compute timer ends ----------

    std::cout << "Mandelbrot computation done in " << runtime_ms << " ms\n";

    write_pbm("mandelbrot.pbm", mask, width, height);
    std::cout << "Saved image to mandelbrot.pbm\n";

    return 0;
}

