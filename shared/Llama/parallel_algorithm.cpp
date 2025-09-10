#include <iostream>
#include <fstream>
#include <vector>
#include <complex>
#include <numeric>
#include <execution>
#include <chrono>
#include <getopt.h>

// Structure to represent a pixel
struct Pixel {
    int x, y;
    int iterations;
};

// Function to generate the Mandelbrot set
std::vector<Pixel> generateMandelbrot(int width, int height, int maxIterations, int numThreads) {
    std::vector<Pixel> pixels(width * height);

    // Set the number of threads for parallel execution
    std::execution::parallel_policy policy{std::execution::par_unseq};
    policy.execution_policy_tag::concurrency_hint = numThreads;

    // Generate the Mandelbrot set in parallel
    std::for_each(policy, std::begin(pixels), std::end(pixels), [&](Pixel& pixel) {
        int x = pixel.x = std::distance(std::begin(pixels), &pixel) % width;
        int y = pixel.y = std::distance(std::begin(pixels), &pixel) / width;

        std::complex<double> c(x * 3.5 / width - 2.5, y * 2.0 / height - 1.0);
        std::complex<double> z(0.0, 0.0);

        int iterations = 0;
        while (std::abs(z) < 2.0 && iterations < maxIterations) {
            z = z * z + c;
            iterations++;
        }

        pixel.iterations = iterations;
    });

    return pixels;
}

// Function to write the PBM file
void writePBM(const std::vector<Pixel>& pixels, int width, int height, int maxIterations, const std::string& filename) {
    std::ofstream file(filename);
    file << "P1\n" << width << " " << height << "\n";

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int index = y * width + x;
            file << (pixels[index].iterations == maxIterations ? "0" : "1") << " ";
        }
        file << "\n";
    }
}

int main(int argc, char* argv[]) {
    int width = 800;
    int height = 800;
    int maxIterations = 255;
    int numThreads = std::thread::hardware_concurrency();
    std::string filename = "mandelbrot.pbm";

    int opt;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
        case 't':
            numThreads = std::stoi(optarg);
            break;
        default:
            std::cerr << "Usage: " << argv[0] << " [-t <num_threads>]" << std::endl;
            return 1;
        }
    }

    // Start timing
    auto start = std::chrono::high_resolution_clock::now();

    // Generate the Mandelbrot set
    std::vector<Pixel> pixels(width * height);
    for (int i = 0; i < width * height; i++) {
        pixels[i].x = i % width;
        pixels[i].y = i / width;
    }
    pixels = generateMandelbrot(width, height, maxIterations, numThreads);

    // Stop timing
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Write the PBM file
    writePBM(pixels, width, height, maxIterations, filename);

    std::cout << "Generated Mandelbrot set in " << duration << " ms" << std::endl;
    std::cout << "Output written to " << filename << std::endl;

    return 0;
}
