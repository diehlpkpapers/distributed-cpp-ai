#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <coroutine>
#include <future>
#include <getopt.h>

// Structure to represent a task
struct Task {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        Task get_return_object() { return Task{handle_type::from_promise(*this)}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() {}
    };

    handle_type h;
    Task(handle_type h) : h(h) {}
    ~Task() { h.destroy(); }
};

// Mandelbrot set configuration
struct Config {
    int width;
    int height;
    int maxIterations;
    double minX;
    double maxX;
    double minY;
    double maxY;
    int numThreads;
};

// Function to generate the Mandelbrot set
Task generateMandelbrot(Config config, std::vector<uint8_t>& pixels, int startRow, int endRow) {
    for (int y = startRow; y < endRow; ++y) {
        for (int x = 0; x < config.width; ++x) {
            double cReal = config.minX + (x / static_cast<double>(config.width)) * (config.maxX - config.minX);
            double cImag = config.minY + (y / static_cast<double>(config.height)) * (config.maxY - config.minY);
            double zReal = 0.0;
            double zImag = 0.0;
            int iteration = 0;

            while (iteration < config.maxIterations && (zReal * zReal + zImag * zImag) < 4.0) {
                double temp = zReal * zReal - zImag * zImag + cReal;
                zImag = 2.0 * zReal * zImag + cImag;
                zReal = temp;
                ++iteration;
            }

            pixels[y * config.width + x] = (iteration == config.maxIterations) ? 0 : 255;
        }
    }
    co_return;
}

// Function to write the PBM file
void writePBM(const std::string& filename, const std::vector<uint8_t>& pixels, int width, int height) {
    std::ofstream file(filename, std::ios::binary);
    file << "P4\n" << width << " " << height << "\n";
    file.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
}

int main(int argc, char* argv[]) {
    Config config = {
        800,  // width
        800,  // height
        255,  // maxIterations
        -2.5, // minX
        1.5,  // maxX
        -1.5, // minY
        1.5,  // maxY
        4     // numThreads
    };

    int opt;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
            case 't':
                config.numThreads = std::stoi(optarg);
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " [-t numThreads]\n";
                return 1;
        }
    }

    std::vector<uint8_t> pixels(config.width * config.height);
    std::vector<std::future<Task>> tasks;

    auto start = std::chrono::high_resolution_clock::now();

    int rowsPerThread = config.height / config.numThreads;
    for (int i = 0; i < config.numThreads; ++i) {
        int startRow = i * rowsPerThread;
        int endRow = (i == config.numThreads - 1) ? config.height : (i + 1) * rowsPerThread;
        tasks.push_back(std::async(std::launch::async, [] (Config config, std::vector<uint8_t>& pixels, int startRow, int endRow) -> Task {
            co_await generateMandelbrot(config, pixels, startRow, endRow);
        }, config, std::ref(pixels), startRow, endRow));
    }

    for (auto& task : tasks) {
        task.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Runtime: " << duration << " ms\n";

    writePBM("mandelbrot.pbm", pixels, config.width, config.height);

    return 0;
}
