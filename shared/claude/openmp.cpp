#include <iostream>
#include <fstream>
#include <vector>
#include <complex>
#include <chrono>
#include <cstring>
#include <omp.h>

class MandelbrotGenerator {
private:
    int width, height;
    int max_iterations;
    double x_min, x_max, y_min, y_max;
    
public:
    MandelbrotGenerator(int w = 1920, int h = 1080, int max_iter = 1000) 
        : width(w), height(h), max_iterations(max_iter),
          x_min(-2.5), x_max(1.0), y_min(-1.25), y_max(1.25) {}
    
    int mandelbrot_iterations(const std::complex<double>& c) {
        std::complex<double> z(0.0, 0.0);
        int iterations = 0;
        
        while (std::abs(z) <= 2.0 && iterations < max_iterations) {
            z = z * z + c;
            iterations++;
        }
        
        return iterations;
    }
    
    void generate_parallel(std::vector<std::vector<int>>& result, int num_threads) {
        omp_set_num_threads(num_threads);
        
        #pragma omp parallel for schedule(dynamic, 1)
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Map pixel coordinates to complex plane
                double real = x_min + (x_max - x_min) * x / (width - 1);
                double imag = y_min + (y_max - y_min) * y / (height - 1);
                
                std::complex<double> c(real, imag);
                result[y][x] = mandelbrot_iterations(c);
            }
        }
    }
    
    void write_pbm(const std::vector<std::vector<int>>& data, const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << filename << " for writing." << std::endl;
            return;
        }
        
        // Write PBM header (P1 format - ASCII bitmap)
        file << "P1\n";
        file << "# Mandelbrot set generated with OpenMP\n";
        file << width << " " << height << "\n";
        
        // Write bitmap data (1 = black, 0 = white)
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Points in the set (max iterations reached) = black (1)
                // Points not in the set = white (0)
                int pixel = (data[y][x] == max_iterations) ? 1 : 0;
                file << pixel;
                if (x < width - 1) file << " ";
            }
            file << "\n";
        }
        
        file.close();
        std::cout << "Mandelbrot set saved to " << filename << std::endl;
    }
    
    int get_width() const { return width; }
    int get_height() const { return height; }
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  -t <num_threads>  Number of OpenMP threads (default: max available)\n";
    std::cout << "  -w <width>        Image width (default: 1920)\n";
    std::cout << "  -h <height>       Image height (default: 1080)\n";
    std::cout << "  -i <iterations>   Maximum iterations (default: 1000)\n";
    std::cout << "  -o <filename>     Output filename (default: mandelbrot.pbm)\n";
    std::cout << "  --help            Show this help message\n";
}

int main(int argc, char* argv[]) {
    int num_threads = omp_get_max_threads();
    int width = 1920;
    int height = 1080;
    int max_iterations = 1000;
    std::string output_filename = "mandelbrot.pbm";
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            num_threads = std::atoi(argv[++i]);
            if (num_threads <= 0) {
                std::cerr << "Error: Number of threads must be positive." << std::endl;
                return 1;
            }
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            width = std::atoi(argv[++i]);
            if (width <= 0) {
                std::cerr << "Error: Width must be positive." << std::endl;
                return 1;
            }
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            height = std::atoi(argv[++i]);
            if (height <= 0) {
                std::cerr << "Error: Height must be positive." << std::endl;
                return 1;
            }
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            max_iterations = std::atoi(argv[++i]);
            if (max_iterations <= 0) {
                std::cerr << "Error: Iterations must be positive." << std::endl;
                return 1;
            }
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_filename = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Error: Unknown option " << argv[i] << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Display configuration
    std::cout << "Mandelbrot Set Generator (Parallel with OpenMP)" << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Resolution: " << width << "x" << height << std::endl;
    std::cout << "  Max iterations: " << max_iterations << std::endl;
    std::cout << "  Threads: " << num_threads << std::endl;
    std::cout << "  Output file: " << output_filename << std::endl;
    std::cout << std::endl;
    
    // Create generator and result matrix
    MandelbrotGenerator generator(width, height, max_iterations);
    std::vector<std::vector<int>> result(height, std::vector<int>(width));
    
    // Measure computation time (excluding I/O)
    std::cout << "Generating Mandelbrot set..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    generator.generate_parallel(result, num_threads);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "Computation completed in " << duration.count() << " ms" << std::endl;
    
    // Write output file (I/O not included in timing)
    std::cout << "Writing output file..." << std::endl;
    generator.write_pbm(result, output_filename);
    
    // Performance summary
    double pixels_per_second = (static_cast<double>(width * height) / duration.count()) * 1000.0;
    std::cout << "\nPerformance Summary:" << std::endl;
    std::cout << "  Total pixels: " << width * height << std::endl;
    std::cout << "  Computation time: " << duration.count() << " ms" << std::endl;
    std::cout << "  Pixels per second: " << static_cast<long long>(pixels_per_second) << std::endl;
    std::cout << "  Threads used: " << num_threads << std::endl;
    
    return 0;
}
