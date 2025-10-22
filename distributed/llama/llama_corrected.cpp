#include <iostream>
#include <fstream>
#include <complex>
#include <vector>
#include <omp.h>
#include <mpi.h>

// Structure to hold image dimensions and max iterations
struct ImageConfig {
    int width;
    int height;
    int maxIterations;
};

// Function to generate Mandelbrot set for a range of rows
void generateMandelbrot(std::complex<double> *pixels, ImageConfig config, int startRow, int endRow) {
    #pragma omp parallel for
    for (int row = startRow; row < endRow; row++) {
        for (int col = 0; col < config.width; col++) {
            std::complex<double> c(-2.5 + (col * 3.5 / config.width), -1.5 + (row * 3.0 / config.height));
            std::complex<double> z(0, 0);
            int iteration = 0;
            while (std::abs(z) < 2 && iteration < config.maxIterations) {
                z = z * z + c;
                iteration++;
            }
            pixels[(row - startRow) * config.width + col] = std::complex<double>(iteration, 0);
        }
    }
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Parse command line options
    int partitions = 1;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-p" && i + 1 < argc) {
            partitions = std::stoi(argv[i + 1]);
        }
    }

    // Define image dimensions and max iterations
    ImageConfig config;
    config.width = 800;
    config.height = 800;
    config.maxIterations = 256;

    // Calculate rows per process
    int rowsPerProcess = config.height / size;
    int startRow = rank * rowsPerProcess;
    int endRow = (rank == size - 1) ? config.height : (rank + 1) * rowsPerProcess;
    int localRows = endRow - startRow;

    // Allocate memory for pixels
    std::complex<double> *localPixels = new std::complex<double>[localRows * config.width];

    // Generate Mandelbrot set
    double startTime = MPI_Wtime();
    generateMandelbrot(localPixels, config, startRow, endRow);
    double endTime = MPI_Wtime();

    // Prepare gather counts and displacements for MPI_Gatherv
    std::vector<int> recvcounts(size), displs(size);
    for (int r = 0; r < size; ++r) {
        int sRow = r * rowsPerProcess;
        int eRow = (r == size - 1) ? config.height : (r + 1) * rowsPerProcess;
        recvcounts[r] = (eRow - sRow) * config.width;
        displs[r] = sRow * config.width;
    }

    // Gather pixels from all processes
    std::complex<double> *pixels = nullptr;
    if (rank == 0) {
        pixels = new std::complex<double>[config.width * config.height];
    }
    MPI_Gatherv(localPixels, localRows * config.width, MPI_DOUBLE_COMPLEX,
                pixels, recvcounts.data(), displs.data(), MPI_DOUBLE_COMPLEX,
                0, MPI_COMM_WORLD);

    // Write to PGM (P2) file
    if (rank == 0) {
        std::ofstream file("mandelbrot.pgm");
        file << "P2\n" << config.width << " " << config.height << "\n" << config.maxIterations << "\n";
        for (int row = 0; row < config.height; row++) {
            for (int col = 0; col < config.width; col++) {
                file << int(pixels[row * config.width + col].real()) << " ";
            }
            file << "\n";
        }
        file.close();
        std::cout << "Runtime: " << endTime - startTime << " seconds\n";
    }

    // Clean up
    delete[] localPixels;
    if (rank == 0) {
        delete[] pixels;
    }

    MPI_Finalize();
    return 0;
}

