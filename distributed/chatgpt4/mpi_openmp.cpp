#include <mpi.h>
#include <omp.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <chrono>

constexpr int WIDTH = 1600;
constexpr int HEIGHT = 1200;
constexpr int MAX_ITER = 1000;
constexpr double XMIN = -2.0, XMAX = 1.0;
constexpr double YMIN = -1.0, YMAX = 1.0;

// Compute Mandelbrot pixel (returns 1 if in set, 0 otherwise)
inline int mandelbrot(double x, double y, int max_iter) {
    double zx = 0, zy = 0;
    int iter = 0;
    while (zx*zx + zy*zy < 4.0 && iter < max_iter) {
        double xtmp = zx*zx - zy*zy + x;
        zy = 2*zx*zy + y;
        zx = xtmp;
        ++iter;
    }
    return (iter == max_iter) ? 1 : 0;
}

// Parse -p argument
int parse_partitions(int argc, char** argv) {
    for (int i = 1; i < argc-1; ++i) {
        if (strcmp(argv[i], "-p") == 0)
            return atoi(argv[i+1]);
    }
    return 0;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    // Optionally override partitions by -p flag
    int partitions = parse_partitions(argc, argv);
    if (partitions && partitions != nprocs) {
        if (rank == 0)
            std::cerr << "Warning: -p " << partitions << " ignored (using MPI size " << nprocs << ")\n";
    }

    // Partition rows
    int rows_per_proc = HEIGHT / nprocs;
    int row_start = rank * rows_per_proc;
    int row_end   = (rank == nprocs-1) ? HEIGHT : (row_start + rows_per_proc);
    int nrows_local = row_end - row_start;

    // Timers (exclude IO)
    MPI_Barrier(MPI_COMM_WORLD);
    auto compute_start = std::chrono::high_resolution_clock::now();

    // Each process: compute its local chunk
    std::vector<unsigned char> local_bitmap(nrows_local * WIDTH, 0);

    #pragma omp parallel for schedule(dynamic)
    for (int j = 0; j < nrows_local; ++j) {
        for (int i = 0; i < WIDTH; ++i) {
            double x = XMIN + (XMAX-XMIN) * i / (WIDTH-1);
            double y = YMIN + (YMAX-YMIN) * (j + row_start) / (HEIGHT-1);
            local_bitmap[j*WIDTH + i] = mandelbrot(x, y, MAX_ITER);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    auto compute_end = std::chrono::high_resolution_clock::now();

    // Gather data to rank 0
    std::vector<unsigned char> full_bitmap;
    if (rank == 0) full_bitmap.resize(WIDTH * HEIGHT);

    std::vector<int> recvcounts(nprocs), displs(nprocs);
    for (int p = 0; p < nprocs; ++p) {
        int s = p * rows_per_proc;
        int e = (p == nprocs-1) ? HEIGHT : (s + rows_per_proc);
        recvcounts[p] = (e - s) * WIDTH;
        displs[p] = s * WIDTH;
    }
    MPI_Gatherv(local_bitmap.data(), nrows_local * WIDTH, MPI_UNSIGNED_CHAR,
                full_bitmap.data(), recvcounts.data(), displs.data(), MPI_UNSIGNED_CHAR,
                0, MPI_COMM_WORLD);

    // Output runtime (excluding IO)
    if (rank == 0) {
        double runtime = std::chrono::duration<double>(compute_end - compute_start).count();
        std::cout << "Compute time (excluding IO): " << runtime << " seconds\n";

        // Write PBM (P4) file
        std::ofstream fout("mandelbrot.pbm", std::ios::binary);
        fout << "P4\n" << WIDTH << " " << HEIGHT << "\n";
        for (int j = 0; j < HEIGHT; ++j) {
            for (int b = 0; b < WIDTH/8; ++b) {
                unsigned char byte = 0;
                for (int bit = 0; bit < 8; ++bit) {
                    byte <<= 1;
                    if (full_bitmap[j*WIDTH + b*8 + bit])
                        byte |= 1;
                }
                fout.put(byte);
            }
        }
        fout.close();
        std::cout << "PBM image written to mandelbrot.pbm\n";
    }

    MPI_Finalize();
    return 0;
}

