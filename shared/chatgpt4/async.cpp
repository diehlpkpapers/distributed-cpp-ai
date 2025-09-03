#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <future>
#include <chrono>
#include <thread>
#include <algorithm>

// Mandelbrot pixel computation
uint8_t mandelbrot(double cr, double ci, int max_iter) {
    double zr = 0.0, zi = 0.0;
    int i = 0;
    while (zr * zr + zi * zi <= 4.0 && i < max_iter) {
        double zr_temp = zr * zr - zi * zi + cr;
        zi = 2.0 * zr * zi + ci;
        zr = zr_temp;
        ++i;
    }
    return (i == max_iter) ? 1 : 0; // 1 = black (inside set), 0 = white (outside)
}

// Write PBM binary (P4) format
void write_pbm(const std::string& filename, const std::vector<uint8_t>& data, int width, int height) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open file for writing: " << filename << "\n";
        return;
    }

    out << "P4\n" << width << " " << height << "\n";
    int row_bytes = (width + 7) / 8;
    std::vector<uint8_t> rowbuf(row_bytes);

    for (int y = 0; y < height; ++y) {
        std::fill(rowbuf.begin(), rowbuf.end(), 0);
        for (int x = 0; x < width; ++x) {
            if (data[y * width + x]) {
                int byte_index = x / 8;
                int bit_index = 7 - (x % 8);
                rowbuf[byte_index] |= (1 << bit_index);
            }
        }
        out.write(reinterpret_cast<const char*>(rowbuf.data()), row_bytes);
    }
}

int main(int argc, char* argv[]) {
    // Defaults
    int width = 192000, height = 10800, max_iter = 1000;
    int threads = std::thread::hardware_concurrency();
    double x_center = -0.75, y_center = 0.0, scale = 3.0;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-t" && i + 1 < argc) {
            threads = std::atoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [-t threads] [width height max_iter x_center y_center scale]\n";
            return 0;
        } else if (i + 1 < argc) {
            if (i == 1) width = std::atoi(argv[i]);
            else if (i == 2) height = std::atoi(argv[i]);
            else if (i == 3) max_iter = std::atoi(argv[i]);
            else if (i == 4) x_center = std::atof(argv[i]);
            else if (i == 5) y_center = std::atof(argv[i]);
            else if (i == 6) scale = std::atof(argv[i]);
        }
    }

    threads = std::max(1, std::min(threads, height)); // clamp threads to rows

    double aspect = static_cast<double>(width) / height;
    double x_min = x_center - scale / 2.0;
    double x_max = x_center + scale / 2.0;
    double y_min = y_center - scale / (2.0 * aspect);
    double y_max = y_center + scale / (2.0 * aspect);
    double dx = (x_max - x_min) / (width - 1);
    double dy = (y_max - y_min) / (height - 1);

    std::vector<uint8_t> image(width * height);

    // Start compute timer
    auto t0 = std::chrono::steady_clock::now();

    // Divide rows among threads
    int rows_per_thread = (height + threads - 1) / threads;
    std::vector<std::future<void>> tasks;

    for (int t = 0; t < threads; ++t) {
        int y_start = t * rows_per_thread;
        int y_end = std::min(height, y_start + rows_per_thread);

        tasks.emplace_back(std::async(std::launch::async, [=, &image]() {
            for (int y = y_start; y < y_end; ++y) {
                double ci = y_max - y * dy;
                for (int x = 0; x < width; ++x) {
                    double cr = x_min + x * dx;
                    image[y * width + x] = mandelbrot(cr, ci, max_iter);
                }
            }
        }));
    }

    for (auto& task : tasks) {
        task.get(); // join
    }

    auto t1 = std::chrono::steady_clock::now();
    double runtime_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "Compute time (excluding I/O): " << runtime_ms << " ms\n";

    // Save image
    write_pbm("mandelbrot.pbm", image, width, height);
    std::cout << "Saved output to mandelbrot.pbm (" << width << "x" << height << ")\n";

    return 0;
}

