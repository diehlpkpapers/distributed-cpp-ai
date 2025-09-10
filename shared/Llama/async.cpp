#include <iostream>
#include <fstream>
#include <vector>
#include <future>
#include <chrono>
#include <getopt.h>

// Structure to represent a pixel
struct Pixel {
    int x, y;
    int iterations;
};

// Function to calculate the Mandelbrot set for a given pixel
Pixel calculatePixel(int x, int y, int width, int height, int maxIterations) {
    double cx = (double)x / width * 3.5 - 2.5;
    double cy = (double)y / height * 2.0 - 1.0;
    double zx = 0.0;
    double zy = 0.0;
    int iterations = 0;

    while (zx * zx + zy * zy < 4.0 && iterations < maxIterations) {
        double temp = zx * zx - zy * zy + cx;
        zy = 2.0 * zx * zy + cy;
        zx = temp;
        iterations++;
    }

    return {x, y, iterations};
}

// Function to generate the Mandelbrot set for a given row
std::vector<Pixel> generateRow(int row, int width, int height, int maxIterations) {
    std::vector<Pixel> pixels;
    for (int x = 0; x < width; x++) {
        pixels.push_back(calculatePixel(x, row, width, height, maxIterations));
    }
    return pixels;
}

int main(int argc, char* argv[]) {
    int width = 800;
    int height = 800;
    int maxIterations = 256;
    int numThreads = std::thread::hardware_concurrency();
    std::string outputFile = "mandelbrot.pbm";

    // Parse command line options
    int opt;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
            case 't':
                numThreads = std::stoi(optarg);
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " [-t numThreads]" << std::endl;
                return 1;
        }
    }

    // Start timer
    auto start = std::chrono::high_resolution_clock::now();

    // Generate Mandelbrot set using async and future
    std::vector<std::future<std::vector<Pixel>>> futures;
    for (int y = 0; y < height; y++) {
        futures.push_back(std::async(std::launch::async, generateRow, y, width, height, maxIterations));
        if (futures.size() >= numThreads) {
            // Wait for one of the futures to complete
            futures.front().wait();
            futures.erase(futures.begin());
        }
    }

    // Wait for all remaining futures to complete
    std::vector<std::vector<Pixel>> pixels;
    for (auto& future : futures) {
        pixels.push_back(future.get());
    }

    // Stop timer
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Runtime: " << duration << " ms" << std::endl;

    // Write output to PBM file
    std::ofstream file(outputFile);
    file << "P1\n" << width << " " << height << "\n";
    for (const auto& row : pixels) {
        for (const auto& pixel : row) {
            file << (pixel.iterations == maxIterations ? "0" : "1") << " ";
        }
        file << "\n";
    }
    file.close();

    return 0;
}
