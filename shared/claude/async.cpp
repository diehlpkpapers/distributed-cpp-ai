#include <iostream>
#include <vector>
#include <future>
#include <chrono>
#include <fstream>
#include <complex>
#include <algorithm>
#include <string>

class MandelbrotGenerator {
private:
    int width, height, max_iterations;
    double min_x, max_x, min_y, max_y;
    
public:
    MandelbrotGenerator(int w = 192000, int h = 10800, int max_iter = 1000) 
        : width(w), height(h), max_iterations(max_iter),
          min_x(-2.5), max_x(1.5), min_y(-1.5), max_y(1.5) {}
    
    // Compute Mandelbrot iteration count for a single point
    int mandelbrot_point(double x, double y) const {
        std::complex<double> c(x, y);
        std::complex<double> z(0.0, 0.0);
        
        for (int i = 0; i < max_iterations; ++i) {
            if (std::abs(z) > 2.0) {
                return i;
            }
            z = z * z + c;
        }
        return max_iterations;
    }
    
    // Convert pixel coordinates to complex plane coordinates
    std::pair<double, double> pixel_to_complex(int px, int py) const {
        double x = min_x + (max_x - min_x) * px / width;
        double y = min_y + (max_y - min_y) * py / height;
        return {x, y};
    }
    
    // Compute a horizontal strip of the Mandelbrot set
    std::vector<int> compute_strip(int start_row, int end_row) const {
        std::vector<int> strip;
        strip.reserve((end_row - start_row) * width);
        
        for (int y = start_row; y < end_row; ++y) {
            for (int x = 0; x < width; ++x) {
                auto [cx, cy] = pixel_to_complex(x, y);
                int iterations = mandelbrot_point(cx, cy);
                strip.push_back(iterations);
            }
        }
        return strip;
    }
    
    // Generate the complete Mandelbrot set using parallel computation
    std::vector<int> generate_parallel(int num_threads) const {
        std::vector<int> result(width * height);
        std::vector<std::future<std::vector<int>>> futures;
        
        // Calculate rows per thread
        int rows_per_thread = height / num_threads;
        int remaining_rows = height % num_threads;
        
        int current_row = 0;
        
        // Launch async tasks for each thread
        for (int t = 0; t < num_threads; ++t) {
            int start_row = current_row;
            int end_row = start_row + rows_per_thread + (t < remaining_rows ? 1 : 0);
            
            futures.push_back(
                std::async(std::launch::async, 
                          &MandelbrotGenerator::compute_strip, 
                          this, start_row, end_row)
            );
            
            current_row = end_row;
        }
        
        // Collect results from all threads
        int result_offset = 0;
        for (auto& future : futures) {
            auto strip = future.get();
            std::copy(strip.begin(), strip.end(), result.begin() + result_offset);
            result_offset += strip.size();
        }
        
        return result;
    }
    
    // Write results to PBM format (Portable Bitmap)
    void write_pbm(const std::vector<int>& data, const std::string& filename) const {
        std::ofstream file(filename);
        if (!file) {
            throw std::runtime_error("Cannot open file for writing: " + filename);
        }
        
        // PBM header
        file << "P1\n";
        file << width << " " << height << "\n";
        
        // Convert iteration counts to binary (0 or 1)
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int iterations = data[y * width + x];
                // Points in the set (max iterations) are black (1), others are white (0)
                file << (iterations == max_iterations ? 1 : 0);
                if (x < width - 1) file << " ";
            }
            file << "\n";
        }
    }
    
    // Getters for image dimensions
    int get_width() const { return width; }
    int get_height() const { return height; }
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  -t <threads>  Number of threads to use (default: 4)\n"
              << "  -w <width>    Image width (default: 800)\n"
              << "  -h <height>   Image height (default: 600)\n"
              << "  -i <iter>     Maximum iterations (default: 1000)\n"
              << "  -o <file>     Output filename (default: mandelbrot.pbm)\n"
              << "  --help        Show this help message\n";
}

int main(int argc, char* argv[]) {
    int num_threads = 4;
    int width = 192000;
    int height = 10800;
    int max_iterations = 1000;
    std::string output_file = "mandelbrot.pbm";
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-t" && i + 1 < argc) {
            num_threads = std::stoi(argv[++i]);
            if (num_threads <= 0) {
                std::cerr << "Error: Number of threads must be positive\n";
                return 1;
            }
        } else if (arg == "-w" && i + 1 < argc) {
            width = std::stoi(argv[++i]);
        } else if (arg == "-h" && i + 1 < argc) {
            height = std::stoi(argv[++i]);
        } else if (arg == "-i" && i + 1 < argc) {
            max_iterations = std::stoi(argv[++i]);
        } else if (arg == "-o" && i + 1 < argc) {
            output_file = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    
    std::cout << "Generating Mandelbrot set:\n";
    std::cout << "  Resolution: " << width << "x" << height << "\n";
    std::cout << "  Max iterations: " << max_iterations << "\n";
    std::cout << "  Threads: " << num_threads << "\n";
    std::cout << "  Output file: " << output_file << "\n\n";
    
    try {
        MandelbrotGenerator generator(width, height, max_iterations);
        
        // Measure computation time (excluding I/O)
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<int> mandelbrot_data = generator.generate_parallel(num_threads);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "Computation completed in: " << duration.count() << " ms\n";
        
        // Write to file (I/O time not included in measurement)
        std::cout << "Writing to " << output_file << "...\n";
        generator.write_pbm(mandelbrot_data, output_file);
        std::cout << "Done!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
