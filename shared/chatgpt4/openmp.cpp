#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <omp.h>

// Mandelbrot evaluation (1 if in set, 0 otherwise)
inline uint8_t mandelbrot(double cr, double ci, int max_iter) {
    double zr = 0.0, zi = 0.0;
    int i = 0;
    while (zr * zr + zi * zi <= 4.0 && i < max_iter) {
        double temp = zr * zr - zi * zi + cr;
        zi = 2.0 * zr * zi + ci;
        zr = temp;
        ++i;
    }
    return (i == max_iter) ? 1 : 0; // 1 = black (inside), 0 = white (outside)
}

// Write PBM binary (P4) format
void write_pbm(const std::string& filename, const std::vector<uint8_t>& data, int width, int height) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        std::cerr << "Error: cannot open " << filename << "\n";
        return;
    }

    out << "P4\n" << width << " " << height << "\n";
    int row_bytes = (width + 7) / 8;
    std::vector<uint8_t> rowbuf(row_bytes);

    for (int y = 0; y < height; ++y) {
        std::fill(rowbuf.begin(), rowbuf.end(), 0);
        for (int x = 0; x < width; ++x) {
            if (data[y * width + x]) {
                rowbuf[x / 8] |= (1 << (7 - (x % 8)));
            }
        }
        out.write(reinterpret_cast<const char*>(rowbuf.data()), row_bytes);
    }
}

int main(int argc, char* argv[]) {
    // Defaults
    int width = 1920, height = 1080, max_iter = 1000;
    double x_center = -0.75, y_center = 0.0, scale = 3.0;
    int threads = omp_get_max_threads();

    // Parse CLI arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-t" && i + 1 < argc) {
            threads = std::atoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [-t threads] [width height max_iter x_center y_center scale]\n";
            return 0;
        } else {
            if (i == 1) width = std::atoi(argv[i]);
            else if (i == 2) height = std::atoi(argv[i]);
            else if (i == 3) max_iter = std::atoi(argv[i]);
            else if (i == 4) x_center = std::atof(argv[i]);
            else if (i == 5) y_center = std::atof(argv[i]);
            else if (i == 6) scale = std::atof(argv[i]);
        }
    }

    omp_set_num_threads(threads);

    // Mapping from pixel to complex plane
    double aspect = static_cast<double>(width) / height;
    double x_min = x_center - scale / 2.0;
    double x_max = x_center + scale / 2.0;
    double y_min = y_center - scale / (2.0 * aspect);
    double y_max = y_center + scale / (2.0 * aspect);
    double dx = (x_max - x_min) / (width - 1);
    double dy = (y_max - y_min) / (height - 1);

    std::vector<uint8_t> image(width * height);

    // --- Start timing compute ---
    auto t0 = std::chrono::steady_clock::now();

    #pragma omp parallel for schedule(dynamic)
    for (int y = 0; y < height; ++y) {
        double ci = y_max - y * dy;
        for (int x = 0; x < width; ++x) {
            double cr = x_min + x * dx;
            image[y * width + x] = mandelbrot(cr, ci, max_iter);
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double runtime_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "Computed Mandelbrot set in " << runtime_ms << " ms with " << threads << " threads\n";

    // --- Write PBM file (not timed) ---
    write_pbm("mandelbrot.pbm", image, width, height);
    std::cout << "Saved output to mandelbrot.pbm (" << width << "x" << height << ")\n";

    return 0;
}

