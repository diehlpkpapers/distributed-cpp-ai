#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>
#include <coroutine>
#include <future>
#include <getopt.h>
#include <cstdint>
#include <string>

struct Task {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        Task get_return_object() { return Task{handle_type::from_promise(*this)}; }
        std::suspend_never  initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend()   noexcept { return {}; }  // keep frame; caller owns destroy
        void unhandled_exception() { std::terminate(); }
        void return_void() noexcept {}
    };

    handle_type h{};

    explicit Task(handle_type hh) : h(hh) {}
    Task(Task&& o) noexcept : h(std::exchange(o.h, {})) {}
    Task& operator=(Task&& o) noexcept {
        if (this != &o) {
            if (h) h.destroy();
            h = std::exchange(o.h, {});
        }
        return *this;
    }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    ~Task() { if (h) h.destroy(); }

    // (Optional) make Task awaitable so `co_await generateMandelbrot(...)` compiles
    bool await_ready() const noexcept { return true; }
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    void await_resume() const noexcept {}
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
    co_return; // now matches promise_type::return_void()
}

void writePBM(const std::string& filename,
              const std::vector<uint8_t>& pixels, // values 0 or 255
              int width, int height)
{
    std::ofstream file(filename, std::ios::binary);
    file << "P4\n" << width << " " << height << "\n";

    const int rowBytes = (width + 7) / 8;
    std::vector<unsigned char> packed(rowBytes * height, 0);

    for (int y = 0; y < height; ++y) {
        unsigned char byte = 0;
        int bit = 7;                       // MSB first
        int outIdx = y * rowBytes;

        for (int x = 0; x < width; ++x) {
            // In PBM: 1 = black, 0 = white. Your pixels are 0 (inside) or 255 (outside).
            // Map: val=0  -> bit=1 (black), val=255 -> bit=0 (white).
            if (pixels[y * width + x] == 0) byte |= (1u << bit);
            if (--bit < 0) {                // emit full byte
                packed[outIdx++] = byte;
                byte = 0;
                bit = 7;
            }
        }
        if (bit != 7) {                     // flush partial byte at end of row
            packed[outIdx] = byte;
        }
    }

    file.write(reinterpret_cast<const char*>(packed.data()), packed.size());
}


int main(int argc, char* argv[]) {
    Config config = {
        192000,  // width
        10800,  // height
        1000,  // maxIterations
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

        tasks.push_back(std::async(std::launch::async,
            [] (Config cfg, std::vector<uint8_t>& pix, int sr, int er) -> Task {
                co_await generateMandelbrot(cfg, pix, sr, er); // OK: Task is awaitable
            },
            config, std::ref(pixels), startRow, endRow));
    }

    for (auto& task : tasks) {
        task.get(); // waits for each async job (and thus its coroutine) to finish
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Runtime: " << duration << " ms\n";

    writePBM("mandelbrot.pbm", pixels, config.width, config.height);

    return 0;
}

