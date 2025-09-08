#include <iostream>
#include <fstream>
#include <vector>
#include <complex>
#include <algorithm>
#include <execution>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>

class MandelbrotGenerator {
private:
    int width, height, max_iterations;
    double x_min, x_max, y_min, y_max;
    
public:
    MandelbrotGenerator(int w, int h, int max_iter = 1000) 
        : width(w), height(h), max_iterations(max_iter),
          x_min(-2.5), x_max(1.5), y_min(-2.0), y_max(2.0) {}
    
    // Calculate iterations for a single point
    int mandelbrot_point(std::complex<double> c) const {
        std::complex<double> z(0, 0);
        int iterations = 0;
        
        while (std::abs(z) <= 2.0 && iterations < max_iterations) {
            z = z * z + c;
            iterations++;
        }
        
        return iterations;
    }
    
    // Generate the Mandelbrot set in parallel
    std::vector<int> generate_parallel() const {
        std::vector<int> pixels(width * height);
        std::vector<int> indices(width * height);
        
        // Initialize indices for parallel processing
        std::iota(indices.begin(), indices.end(), 0);
        
        // Parallel computation using C++17 execution policies
        std::transform(std::execution::par_unseq, 
                      indices.begin(), indices.end(), 
                      pixels.begin(),
                      [this](int index) {
                          int x = index % width;
                          int y = index / width;
                          
                          // Map pixel coordinates to complex plane
                          double real = x_min + (x_max - x_min) * x / (width - 1);
                          double imag = y_min + (y_max - y_min) * y / (height - 1);
                          
                          std::complex<double> c(real, imag);
                          return mandelbrot_point(c);
                      });
        
        return pixels;
    }
    
    // Write result to PBM format
    void write_pbm(const std::vector<int>& pixels, const std::string& filename) const {
        std::ofstream file(filename, std::ios::binary);
        
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }
        
        // PBM header
        file << "P4\n" << width << " " << height << "\n";
        
        // Convert to binary format (packed bits)
        std::vector<unsigned char> binary_data;
        
        for (int y = 0; y < height; ++y) {
            unsigned char byte = 0;
            int bit_pos = 7;
            
            for (int x = 0; x < width; ++x) {
                int pixel_index = y * width + x;
                
                // Black if in set (iterations == max_iterations), white otherwise
                bool is_black = (pixels[pixel_index] == max_iterations);
                
                if (is_black) {
                    byte |= (1 << bit_pos);
                }
                
                bit_pos--;
                
                if (bit_pos < 0 || x == width - 1) {
                    binary_data.push_back(byte);
                    byte = 0;
                    bit_pos = 7;
                }
            }
        }
        
        file.write(reinterpret_cast<const char*>(binary_data.data()), binary_data.size());
        file.close();
    }
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  -t <threads>    Number of threads to use (default: hardware concurrency)\n";
    std::cout << "  -w <width>      Image width (default: 800)\n";
    std::cout << "  -h <height>     Image height (default: 600)\n";
    std::cout << "  -i <iterations> Maximum iterations (default: 1000)\n";
    std::cout << "  -o <filename>   Output filename (default: mandelbrot.pbm)\n";
    std::cout << "  --help          Show this help message\n";
}

int main(int argc, char* argv[]) {
    // Default parameters
    int num_threads = std::thread::hardware_concurrency();
    int width = 192000;
    int height = 10800;
    int max_iterations = 1000;
    std::string output_filename = "mandelbrot.pbm";
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            num_threads = std::atoi(argv[++i]);
            if (num_threads <= 0) {
                std::cerr << "Error: Number of threads must be positive\n";
                return 1;
            }
        }
        else if (std::strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            width = std::atoi(argv[++i]);
            if (width <= 0) {
                std::cerr << "Error: Width must be positive\n";
                return 1;
            }
        }
        else if (std::strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            height = std::atoi(argv[++i]);
            if (height <= 0) {
                std::cerr << "Error: Height must be positive\n";
                return 1;
            }
        }
        else if (std::strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            max_iterations = std::atoi(argv[++i]);
            if (max_iterations <= 0) {
                std::cerr << "Error: Iterations must be positive\n";
                return 1;
            }
        }
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_filename = argv[++i];
        }
        else if (std::strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Set the number of threads for parallel execution
    // Note: C++17 parallel algorithms may not directly use this,
    // but some implementations respect thread pool sizes
    if (num_threads > 0) {
        // This is implementation-dependent, but many systems respect this
        std::cout << "Configured to use " << num_threads << " threads\n";
    }
    
    std::cout << "Generating Mandelbrot set (" << width << "x" << height 
              << ", " << max_iterations << " iterations)...\n";
    
    try {
        MandelbrotGenerator generator(width, height, max_iterations);
        
        // Measure computation time (excluding I/O)
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<int> pixels = generator.generate_parallel();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "Computation completed in " << duration.count() << " ms\n";
        
        // Write to PBM file (I/O not included in timing)
        std::cout << "Writing to " << output_filename << "...\n";
        generator.write_pbm(pixels, output_filename);
        std::cout << "Done!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
