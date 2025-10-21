#include <mpi.h>
#include <omp.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <complex>
#include <cstdlib>
#include <cstring>

const int WIDTH = 4096;
const int HEIGHT = 4096;
const int MAX_ITER = 1000;
const double X_MIN = -2.5;
const double X_MAX = 1.0;
const double Y_MIN = -1.25;
const double Y_MAX = 1.25;

int mandelbrot(std::complex<double> c) {
    std::complex<double> z = 0;
    int iter = 0;
    
    while (abs(z) <= 2.0 && iter < MAX_ITER) {
        z = z * z + c;
        iter++;
    }
    
    return iter;
}

void compute_rows(int start_row, int end_row, std::vector<unsigned char>& buffer) {
    int width = WIDTH;
    int height = HEIGHT;
    
    #pragma omp parallel for schedule(dynamic)
    for (int row = start_row; row < end_row; row++) {
        int local_row = row - start_row;
        for (int col = 0; col < width; col++) {
            double x = X_MIN + (X_MAX - X_MIN) * col / (width - 1);
            double y = Y_MIN + (Y_MAX - Y_MIN) * row / (height - 1);
            
            std::complex<double> c(x, y);
            int iter = mandelbrot(c);
            
            // Map iteration count to grayscale (0-255)
            unsigned char value = (iter == MAX_ITER) ? 0 : (255 * iter / MAX_ITER);
            buffer[local_row * width + col] = value;
        }
    }
}

void write_pbm(const std::vector<unsigned char>& image, const char* filename) {
    std::ofstream file(filename, std::ios::binary);
    
    // Write PGM header (using PGM instead of PBM for grayscale)
    file << "P5\n" << WIDTH << " " << HEIGHT << "\n255\n";
    
    // Write pixel data
    file.write(reinterpret_cast<const char*>(image.data()), image.size());
    file.close();
}

int main(int argc, char* argv[]) {
    int rank, size;
    int num_partitions = 1;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            num_partitions = atoi(argv[i + 1]);
            i++;
        }
    }
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (num_partitions < 1 || num_partitions > HEIGHT) {
        if (rank == 0) {
            std::cerr << "Invalid number of partitions. Using default (1)." << std::endl;
        }
        num_partitions = 1;
    }
    
    // Calculate rows per partition
    int rows_per_partition = (HEIGHT + num_partitions - 1) / num_partitions;
    int partitions_per_process = (num_partitions + size - 1) / size;
    
    std::vector<unsigned char> local_data;
    std::vector<unsigned char> full_image;
    
    if (rank == 0) {
        full_image.resize(WIDTH * HEIGHT);
    }
    
    double start_time = MPI_Wtime();
    
    // Each process handles multiple partitions
    for (int p = 0; p < partitions_per_process; p++) {
        int partition_id = rank * partitions_per_process + p;
        
        if (partition_id >= num_partitions) break;
        
        int start_row = partition_id * rows_per_partition;
        int end_row = std::min(start_row + rows_per_partition, HEIGHT);
        int num_rows = end_row - start_row;
        
        if (num_rows <= 0) continue;
        
        std::vector<unsigned char> partition_buffer(num_rows * WIDTH);
        compute_rows(start_row, end_row, partition_buffer);
        
        // Gather results to rank 0
        if (rank == 0) {
            std::copy(partition_buffer.begin(), partition_buffer.end(),
                     full_image.begin() + start_row * WIDTH);
        } else {
            MPI_Send(partition_buffer.data(), num_rows * WIDTH, MPI_UNSIGNED_CHAR,
                    0, partition_id, MPI_COMM_WORLD);
        }
    }
    
    // Rank 0 receives data from all other processes
    if (rank == 0) {
        for (int src = 1; src < size; src++) {
            for (int p = 0; p < partitions_per_process; p++) {
                int partition_id = src * partitions_per_process + p;
                
                if (partition_id >= num_partitions) break;
                
                int start_row = partition_id * rows_per_partition;
                int end_row = std::min(start_row + rows_per_partition, HEIGHT);
                int num_rows = end_row - start_row;
                
                if (num_rows <= 0) continue;
                
                MPI_Recv(full_image.data() + start_row * WIDTH, num_rows * WIDTH,
                        MPI_UNSIGNED_CHAR, src, partition_id, MPI_COMM_WORLD,
                        MPI_STATUS_IGNORE);
            }
        }
    }
    
    double end_time = MPI_Wtime();
    
    // Write output and print timing
    if (rank == 0) {
        std::cout << "Computation time (excluding I/O): " 
                  << (end_time - start_time) << " seconds" << std::endl;
        std::cout << "Number of partitions: " << num_partitions << std::endl;
        std::cout << "Number of MPI processes: " << size << std::endl;
        
        write_pbm(full_image, "mandelbrot.pgm");
        std::cout << "Output written to mandelbrot.pgm" << std::endl;
    }
    
    MPI_Finalize();
    return 0;
}
