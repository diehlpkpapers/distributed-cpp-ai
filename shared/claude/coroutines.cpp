#include <iostream>
#include <fstream>
#include <vector>
#include <complex>
#include <thread>
#include <future>
#include <chrono>
#include <coroutine>
#include <memory>
#include <cstring>
#include <algorithm>

// Coroutine task type
template<typename T>
struct Task {
    struct promise_type {
        T value;
        
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        
        void return_value(T val) {
            value = std::move(val);
        }
        
        void unhandled_exception() {
            std::terminate();
        }
    };
    
    std::coroutine_handle<promise_type> h;
    
    Task(std::coroutine_handle<promise_type> handle) : h(handle) {}
    
    ~Task() {
        if (h) h.destroy();
    }
    
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    Task(Task&& other) noexcept : h(std::exchange(other.h, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (h) h.destroy();
            h = std::exchange(other.h, {});
        }
        return *this;
    }
    
    T get() {
        return h.promise().value;
    }
    
    bool done() const {
        return h.done();
    }
};

// Configuration structure
struct MandelbrotConfig {
    int width = 192000;
    int height = 10800;
    double x_min = -2.5;
    double x_max = 1.0;
    double y_min = -1.2;
    double y_max = 1.2;
    int max_iterations = 1000;
    int num_threads = std::thread::hardware_concurrency();
};

// Calculate Mandelbrot iterations for a single point
int mandelbrot_point(std::complex<double> c, int max_iter) {
    std::complex<double> z = 0;
    int iter = 0;
    
    while (std::abs(z) <= 2.0 && iter < max_iter) {
        z = z * z + c;
        ++iter;
    }
    
    return iter;
}

// Coroutine that computes a row of the Mandelbrot set
Task<std::vector<int>> compute_row(int row, const MandelbrotConfig& config) {
    std::vector<int> result(config.width);
    
    double y = config.y_min + (config.y_max - config.y_min) * row / (config.height - 1.0);
    
    for (int col = 0; col < config.width; ++col) {
        double x = config.x_min + (config.x_max - config.x_min) * col / (config.width - 1.0);
        std::complex<double> c(x, y);
        result[col] = mandelbrot_point(c, config.max_iterations);
    }
    
    co_return result;
}

// Worker function that processes multiple rows
std::vector<std::pair<int, std::vector<int>>> worker_function(
    const std::vector<int>& rows, 
    const MandelbrotConfig& config) {
    
    std::vector<std::pair<int, std::vector<int>>> results;
    std::vector<Task<std::vector<int>>> tasks;
    
    // Launch coroutines for assigned rows
    for (int row : rows) {
        tasks.push_back(compute_row(row, config));
    }
    
    // Collect results
    for (size_t i = 0; i < tasks.size(); ++i) {
        while (!tasks[i].done()) {
            std::this_thread::yield();
        }
        results.emplace_back(rows[i], tasks[i].get());
    }
    
    return results;
}

// Generate the Mandelbrot set using parallel coroutines
std::vector<std::vector<int>> generate_mandelbrot(const MandelbrotConfig& config) {
    std::vector<std::vector<int>> image(config.height, std::vector<int>(config.width));
    std::vector<std::future<std::vector<std::pair<int, std::vector<int>>>>> futures;
    
    // Distribute rows among threads
    int rows_per_thread = config.height / config.num_threads;
    int remaining_rows = config.height % config.num_threads;
    
    int current_row = 0;
    for (int t = 0; t < config.num_threads; ++t) {
        std::vector<int> thread_rows;
        
        int rows_for_this_thread = rows_per_thread + (t < remaining_rows ? 1 : 0);
        
        for (int i = 0; i < rows_for_this_thread; ++i) {
            thread_rows.push_back(current_row++);
        }
        
        if (!thread_rows.empty()) {
            futures.push_back(
                std::async(std::launch::async, worker_function, 
                          std::move(thread_rows), std::ref(config))
            );
        }
    }
    
    // Collect results from all threads
    for (auto& future : futures) {
        auto thread_results = future.get();
        for (const auto& [row, row_data] : thread_results) {
            image[row] = std::move(row_data);
        }
    }
    
    return image;
}

// Write image to PBM format
void write_pbm(const std::string& filename, 
               const std::vector<std::vector<int>>& image, 
               const MandelbrotConfig& config) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }
    
    // PBM header
    file << "P1\n";
    file << config.width << " " << config.height << "\n";
    
    // Write pixel data (1 for points in set, 0 for points outside)
    for (int row = 0; row < config.height; ++row) {
        for (int col = 0; col < config.width; ++col) {
            // Points that reached max_iterations are considered in the set
            int pixel = (image[row][col] == config.max_iterations) ? 1 : 0;
            file << pixel;
            if (col < config.width - 1) file << " ";
        }
        file << "\n";
    }
}

// Parse command line arguments
MandelbrotConfig parse_arguments(int argc, char* argv[]) {
    MandelbrotConfig config;
    
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            config.num_threads = std::atoi(argv[i + 1]);
            if (config.num_threads <= 0) {
                config.num_threads = 1;
            }
            ++i; // Skip next argument
        } else if (std::strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            config.width = std::atoi(argv[i + 1]);
            ++i;
        } else if (std::strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            config.height = std::atoi(argv[i + 1]);
            ++i;
        } else if (std::strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            config.max_iterations = std::atoi(argv[i + 1]);
            ++i;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  -t <threads>     Number of threads (default: hardware concurrency)\n";
            std::cout << "  -w <width>       Image width (default: 800)\n";
            std::cout << "  -h <height>      Image height (default: 600)\n";
            std::cout << "  -i <iterations>  Max iterations (default: 1000)\n";
            std::cout << "  --help           Show this help message\n";
            std::exit(0);
        }
    }
    
    return config;
}

int main(int argc, char* argv[]) {
    try {
        MandelbrotConfig config = parse_arguments(argc, argv);
        
        std::cout << "Generating Mandelbrot set with:\n";
        std::cout << "  Dimensions: " << config.width << "x" << config.height << "\n";
        std::cout << "  Max iterations: " << config.max_iterations << "\n";
        std::cout << "  Threads: " << config.num_threads << "\n\n";
        
        // Measure computation time (excluding I/O)
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<std::vector<int>> mandelbrot_image = generate_mandelbrot(config);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        std::cout << "Computation completed in: " << duration.count() << " ms\n";
        
        // Write to PBM file
        std::cout << "Writing to mandelbrot.pbm...\n";
        write_pbm("mandelbrot.pbm", mandelbrot_image, config);
        std::cout << "Done!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
