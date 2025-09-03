#include <coroutine>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <fstream>
#include <iostream>
#include <latch>
#include <chrono>
#include <cstring>
#include <cmath>

// ------------------ Thread Pool ------------------
class thread_pool {
public:
    explicit thread_pool(size_t n) {
        for (size_t i = 0; i < n; ++i) {
            workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> job;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        cv.wait(lock, [this] { return stop || !jobs.empty(); });
                        if (stop && jobs.empty()) return;
                        job = std::move(jobs.front());
                        jobs.pop();
                    }
                    job();
                }
            });
        }
    }

    ~thread_pool() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            stop = true;
        }
        cv.notify_all();
        for (auto& t : workers) t.join();
    }

    void enqueue(std::coroutine_handle<> h) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            jobs.push([h] { h.resume(); });
        }
        cv.notify_one();
    }

    struct schedule_awaitable {
        thread_pool& pool;
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> h) const { pool.enqueue(h); }
        void await_resume() const noexcept {}
    };

    schedule_awaitable schedule() { return schedule_awaitable{*this}; }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> jobs;
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool stop = false;
};

// ------------------ Coroutine Task ------------------
struct task {
    struct promise_type {
        task get_return_object() {
            return task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> coro;
    task(std::coroutine_handle<promise_type> h) : coro(h) {}
    task(const task&) = delete;
    task& operator=(const task&) = delete;
    task(task&& other) noexcept : coro(other.coro) { other.coro = {}; }
    ~task() { if (coro) coro.destroy(); }

    void start() { coro.resume(); }
};

// ------------------ Mandelbrot Coroutine ------------------
task compute_row(thread_pool& pool, std::latch& latch, std::vector<uint8_t>& bitmap,
                 int y, int width, int height,
                 double x_min, double x_max, double y_min, double y_max,
                 int max_iter) {
    co_await pool.schedule();

    double dx = (x_max - x_min) / (width - 1);
    double dy = (y_max - y_min) / (height - 1);

    for (int x = 0; x < width; ++x) {
        double cr = x_min + x * dx;
        double ci = y_max - y * dy;
        double zr = 0.0, zi = 0.0;
        int i = 0;
        for (; i < max_iter && zr*zr + zi*zi <= 4.0; ++i) {
            double temp = zr*zr - zi*zi + cr;
            zi = 2.0*zr*zi + ci;
            zr = temp;
        }
        bitmap[y * width + x] = (i == max_iter) ? 1 : 0;
    }

    latch.count_down();
    co_return;
}

// ------------------ Utility ------------------
void write_pbm(const std::string& filename, const std::vector<uint8_t>& bitmap, int width, int height) {
    std::ofstream out(filename, std::ios::binary);
    out << "P4\n" << width << " " << height << "\n";

    int row_bytes = (width + 7) / 8;
    std::vector<unsigned char> rowbuf(row_bytes);

    for (int y = 0; y < height; ++y) {
        std::fill(rowbuf.begin(), rowbuf.end(), 0);
        for (int x = 0; x < width; ++x) {
            if (bitmap[y * width + x]) {
                rowbuf[x / 8] |= (1 << (7 - (x % 8)));
            }
        }
        out.write(reinterpret_cast<const char*>(rowbuf.data()), row_bytes);
    }
}

// ------------------ Main ------------------
int main(int argc, char* argv[]) {
    int width = 192000, height = 10800, max_iter = 1000, threads = std::thread::hardware_concurrency();
    double x_center = -0.75, y_center = 0.0, scale = 3.0;

    // Parse -t
    for (int i = 1; i < argc - 1; ++i) {
        if (std::strcmp(argv[i], "-t") == 0) {
            threads = std::atoi(argv[i + 1]);
        }
    }

    // Compute bounds
    double aspect = static_cast<double>(width) / height;
    double x_min = x_center - scale / 2;
    double x_max = x_center + scale / 2;
    double y_min = y_center - scale / (2 * aspect);
    double y_max = y_center + scale / (2 * aspect);

    // Allocate output
    std::vector<uint8_t> bitmap(width * height);

    // Start thread pool and latch
    thread_pool pool(threads);
    std::latch done(height);

    std::vector<task> tasks;
    tasks.reserve(height);

    // --- Timing Start ---
    auto t0 = std::chrono::high_resolution_clock::now();

    for (int y = 0; y < height; ++y) {
        tasks.emplace_back(compute_row(pool, done, bitmap, y, width, height,
                                       x_min, x_max, y_min, y_max, max_iter));
    }
    for (auto& t : tasks) t.start();
    done.wait();

    auto t1 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = t1 - t0;
    // --- Timing End ---

    std::cout << "Computed Mandelbrot set in " << elapsed.count() << " seconds using "
              << threads << " threads.\n";

    write_pbm("mandelbrot.pbm", bitmap, width, height);
    std::cout << "Saved image to mandelbrot.pbm\n";
    return 0;
}

