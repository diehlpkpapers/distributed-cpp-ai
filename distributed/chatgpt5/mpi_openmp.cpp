// mandelbrot_mpi_omp.cpp
#include <mpi.h>
#include <omp.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <iostream>

struct Args {
    int    W = 1200;
    int    H = 800;
    int    max_iter = 1000;
    double x0 = -2.0, x1 = 1.0;
    double y0 = -1.2, y1 = 1.2;
    int    partitions = -1;          // default: will become MPI_size
    std::string out = "mandelbrot.pbm";
};

static void usage(const char* prog) {
    if (prog == nullptr) prog = "mandelbrot";
    std::fprintf(stderr,
        "Usage: %s [-W width] [-H height] [-i max_iter]\n"
        "           [-x0 xmin] [-x1 xmax] [-y0 ymin] [-y1 ymax]\n"
        "           [-p partitions] [-o output.pbm]\n", prog);
}

// Very small helper: parse command line (simple, no combined flags)
static Args parse_args(int argc, char** argv, int world_size) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        auto need = [&](int i){ if (i+1 >= argc) { usage(argv[0]); std::exit(1);} };
        if (s == "-W") { need(i); a.W = std::atoi(argv[++i]); }
        else if (s == "-H") { need(i); a.H = std::atoi(argv[++i]); }
        else if (s == "-i") { need(i); a.max_iter = std::atoi(argv[++i]); }
        else if (s == "-x0") { need(i); a.x0 = std::atof(argv[++i]); }
        else if (s == "-x1") { need(i); a.x1 = std::atof(argv[++i]); }
        else if (s == "-y0") { need(i); a.y0 = std::atof(argv[++i]); }
        else if (s == "-y1") { need(i); a.y1 = std::atof(argv[++i]); }
        else if (s == "-p")  { need(i); a.partitions = std::atoi(argv[++i]); }
        else if (s == "-o")  { need(i); a.out = argv[++i]; }
        else { usage(argv[0]); std::exit(1); }
    }
    if (a.W <= 0 || a.H <= 0 || a.max_iter < 1) { usage(argv[0]); std::exit(1); }
    if (a.partitions <= 0) a.partitions = world_size; // default: at least #ranks
    if (a.partitions > a.H) a.partitions = a.H;       // no empty row blocks
    return a;
}

struct Part {
    int id;
    int row0;
    int nrows;
};

static void build_partitions(int H, int P, std::vector<Part>& parts) {
    parts.resize(P);
    for (int j = 0; j < P; ++j) {
        int r0 = (long long)j * H / P;
        int r1 = (long long)(j + 1) * H / P;
        parts[j] = Part{ j, r0, r1 - r0 };
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank = 0, size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    Args args = parse_args(argc, argv, size);

    // Precompute partitions (row blocks)
    std::vector<Part> parts;
    build_partitions(args.H, args.partitions, parts);

    // Compute scaling
    const double dx = (args.x1 - args.x0) / (args.W - 1);
    const double dy = (args.y1 - args.y0) / (args.H - 1);

    // Root will assemble full image (H x W) of uint8 (0/1 for PBM P1)
    std::vector<unsigned char> image;
    if (rank == 0) image.assign((size_t)args.W * args.H, 0);

    // Compute+comm timing (exclude file I/O)
    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();

    // Local computation for the partitions owned by this rank (id % size == rank)
    // We’ll send each computed block to root with tag == partition id.
    std::vector<unsigned char> block; // reused buffer per partition

    for (const auto& p : parts) {
        if (p.id % size != rank) continue;

        block.assign((size_t)p.nrows * args.W, 0);

        // For each row in this block…
        for (int local_r = 0; local_r < p.nrows; ++local_r) {
            int y = p.row0 + local_r;
            double cy = args.y0 + y * dy;

            // OpenMP parallelism across columns for this row
            #pragma omp parallel for schedule(static)
            for (int x = 0; x < args.W; ++x) {
                double cx = args.x0 + x * dx;
                double zx = 0.0, zy = 0.0;
                int it = 0;
                // Iterate z_{n+1} = z_n^2 + c
                while (zx*zx + zy*zy <= 4.0 && it < args.max_iter) {
                    double zx2 = zx*zx - zy*zy + cx;
                    double zy2 = 2.0*zx*zy + cy;
                    zx = zx2; zy = zy2; ++it;
                }
                // PBM: 1 is black, 0 is white. Inside set (hit max_iter) -> black.
                block[(size_t)local_r * args.W + x] = (it == args.max_iter) ? 1 : 0;
            }
        }

        if (rank == 0) {
            // Place directly into final image
            for (int rr = 0; rr < p.nrows; ++rr) {
                std::memcpy(&image[(size_t)(p.row0 + rr) * args.W],
                            &block[(size_t)rr * args.W],
                            (size_t)args.W * sizeof(unsigned char));
            }
        } else {
            // Send to root (tag = partition id)
            MPI_Send(block.data(), p.nrows * args.W, MPI_UNSIGNED_CHAR, 0, p.id, MPI_COMM_WORLD);
        }
    }

    if (rank == 0) {
        // Receive blocks computed by other ranks
        for (const auto& p : parts) {
            int owner = p.id % size;
            if (owner == 0) continue;
            std::vector<unsigned char> recvbuf((size_t)p.nrows * args.W);
            MPI_Recv(recvbuf.data(), p.nrows * args.W, MPI_UNSIGNED_CHAR, owner, p.id, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int rr = 0; rr < p.nrows; ++rr) {
                std::memcpy(&image[(size_t)(p.row0 + rr) * args.W],
                            &recvbuf[(size_t)rr * args.W],
                            (size_t)args.W * sizeof(unsigned char));
            }
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double t1 = MPI_Wtime();
    double local_elapsed = t1 - t0;
    double max_elapsed = 0.0;
    MPI_Reduce(&local_elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // Root writes PBM (ASCII P1). This I/O is *excluded* from the printed runtime.
    if (rank == 0) {
        FILE* f = std::fopen(args.out.c_str(), "wb");
        if (!f) {
            std::perror("fopen");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        // PBM header
        std::fprintf(f, "P1\n%d %d\n", args.W, args.H);
        // PBM data: 0/1 values, whitespace-separated. Write one row per line.
        for (int y = 0; y < args.H; ++y) {
            const unsigned char* row = &image[(size_t)y * args.W];
            for (int x = 0; x < args.W; ++x) {
                std::fputc(row[x] ? '1' : '0', f);
                if (x + 1 < args.W) std::fputc(' ', f);
            }
            std::fputc('\n', f);
        }
        std::fclose(f);

        std::printf("Runtime (compute + MPI, excluding I/O): %.6f seconds\n", max_elapsed);
        std::fflush(stdout);
    }

    MPI_Finalize();
    return 0;
}

