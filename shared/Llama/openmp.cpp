#include <iostream>
#include <fstream>
#include <omp.h>
#include <getopt.h>

// Structure to represent complex numbers
struct Complex {
    double real;
    double imag;
};

// Function to generate the Mandelbrot set
void generateMandelbrot(int width, int height, int maxIterations, int* pixels, double xmin, double xmax, double ymin, double ymax) {
    double scaleX = (xmax - xmin) / width;
    double scaleY = (ymax - ymin) / height;

    #pragma omp parallel for schedule(dynamic)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            Complex c = {xmin + x * scaleX, ymin + y * scaleY};
            Complex z = {0, 0};
            int iteration = 0;

            while (iteration < maxIterations && (z.real * z.real + z.imag * z.imag) < 4) {
                Complex temp = {z.real * z.real - z.imag * z.imag + c.real, 2 * z.real * z.imag + c.imag};
                z = temp;
                iteration++;
            }

            pixels[y * width + x] = (iteration == maxIterations) ? 0 : 1;
        }
    }
}

// Function to write the PBM file
void writePBM(const char* filename, int width, int height, int* pixels) {
    std::ofstream file(filename);
    file << "P1\n" << width << " " << height << "\n";
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            file << pixels[y * width + x] << " ";
        }
        file << "\n";
    }
    file.close();
}

int main(int argc, char** argv) {
    int width = 192000;
    int height = 10800;
    int maxIterations = 1000;
    int numThreads = 4;
    double xmin = -2.5;
    double xmax = 1.5;
    double ymin = -1.5;
    double ymax = 1.5;
    const char* outputFilename = "mandelbrot.pbm";

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

    omp_set_num_threads(numThreads);

    int* pixels = new int[width * height];

    double startTime = omp_get_wtime();
    generateMandelbrot(width, height, maxIterations, pixels, xmin, xmax, ymin, ymax);
    double endTime = omp_get_wtime();

    writePBM(outputFilename, width, height, pixels);

    std::cout << "Runtime (excluding IO): " << endTime - startTime << " seconds" << std::endl;

    delete[] pixels;
    return 0;
}
