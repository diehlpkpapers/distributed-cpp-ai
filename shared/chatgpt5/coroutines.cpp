// mandelbrot_coroutines_pbm_timed.cpp
#include <coroutine>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <latch>
#include <fstream>
#include <cstdint>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <string>
#include <chrono>

// ------------------ thread pool ------------------
class thread_pool {
public:
    explicit thread_pool(std::size_t n) : stop_(false) {
        if (n == 0) n = 1;
        workers_.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            workers_.emplace_back([this] {
                for (;;) {
                    std::function<void()> job;
                    {
                        std::unique_lock<std::mutex> lk(m_);
                        cv_.wait(lk, [this]{ return stop_ || !q_.empty(); });
                        if (stop_ && q_.empty()) return;
                        job = std::move(q_.front()); q_.pop();
                    }
                    job();
                }
            });
        }
    }
    ~thread_pool() {
        { std::lock_guard<std::mutex> lk(m_); stop_ = true; }
        cv_.notify_all();
        for (auto &t : workers_) t.join();
    }
    void enqueue(std::function<void()> f) {
        { std::lock_guard<std::mutex> lk(m_); q_.push(std::move(f)); }
        cv_.notify_one();
    }
    void enqueue(std::coroutine_handle<> h) { enqueue([h]{ h.resume(); }); }

    struct schedule_awaitable {
        thread_pool& pool;
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> h) const { pool.enqueue(h); }
        void await_resume() const noexcept {}
    };
    schedule_awaitable schedule() { return schedule_awaitable{*this}; }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> q_;
    std::mutex m_;
    std::condition_variable cv_;
    bool stop_;
};

// ------------------ minimal task ------------------
struct task {
    struct promise_type {
        task get_return_object() noexcept {
            return task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
    };
    using handle_t = std::coroutine_handle<promise_type>;
    handle_t h{};
    explicit task(handle_t hh) : h(hh) {}
    task(task&& o) noexcept : h(std::exchange(o.h, {})) {}
    task& operator=(task&& o) noexcept {
        if (this != &o) { if (h) h.destroy(); h = std::exchange(o.h, {}); }
        return *this;
    }
    task(const task&) = delete;
    task& operator=(const task&) = delete;
    ~task() {}
    void start() { h.resume(); }
    void destroy() { if (h) { h.destroy(); h = {}; } }
};

// ------------------ coroutine worker ------------------
task render_row(
    thread_pool& pool,
    std::latch& done,
    std::vector<uint8_t>& mask, // PBM: 1=black (inside), 0=white (outside)
    int y, int width, int height,
    double x_min, double x_max, double y_min, double y_max,
    int max_iter)
{
    co_await pool.schedule();

    const double dx = (x_max - x_min) / (width - 1);
    const double dy = (y_max - y_min) / (height - 1);

    for (int x = 0; x < width; ++x) {
        const double cr = x_min + x * dx;
        const double ci = y_max - y * dy; // image Y downward
        double zr = 0.0, zi = 0.0;
        int i = 0;
        for (; i < max_iter && (zr*zr + zi*zi) <= 4.0; ++i) {
            const double zr_new = zr*zr - zi*zi + cr;
            zi = 2.0*zr*zi + ci;
            zr = zr_new;
        }
        mask[static_cast<std::size_t>(y)*width + x] = (i >= max_iter) ? 1u : 0u;
    }

    done.count_down();
    co_return;
}

// ------------------ args ------------------
struct Args {
    int width  = 1920;
    int height = 1080;
    int max_iter = 1000;
    double x_center = -0.75;
    double y_center = 0.0;
    double scale    = 3.0; // complex-plane width
    int threads = static_cast<int>(std::thread::hardware_concurrency());
};

void usage(const char* prog) {
    std::cerr <<
      "Usage: " << prog << " [-t threads] [width height [max_iter [x_center y_center [scale]]]]\n"
      "Defaults: 1920 1080 1000 -0.75 0.0 3.0; threads=hardware_concurrency\n";
}

Args parse_args(int argc, char** argv) {
    Args a;
    std::vector<std::string> pos;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s == "-t") {
            if (i + 1 >= argc) { usage(argv[0]); std::exit(1); }
            int t = std::atoi(argv[++i]);
            if (t > 0) a.threads = t;
        } else if (s == "-h" || s == "--help") {
            usage(argv[0]); std::exit(0);
        } else {
            pos.push_back(std::move(s));
        }
    }
    if (!pos.empty()) {
        a.width  = std::max(16, std::atoi(pos[0].c_str()));
        if (pos.size() >= 2) a.height = std::max(16, std::atoi(pos[1].c_str()));
    }
    if (pos.size() >= 3) a.max_iter = std::max(10, std::atoi(pos[2].c_str()));
    if (pos.size() >= 5) { a.x_center = std::atof(pos[3].c_str()); a.y_center = std::atof(pos[4].c_str()); }
    if (pos.size() >= 6) a.scale = std::atof(pos[5].c_str());
    if (a.threads <= 0) a.threads = 1;
    return a;
}

// ------------------ main ------------------
int main(int argc, char** argv) {
    Args args = parse_args(argc, argv);
    args.threads = std::min(args.threads, std::max(1, args.height)); // avoid > rows

    const double aspect = static_cast<double>(args.width) / args.height;
    const double x_min = args.x_center - 0.5 * args.scale;
    const double x_max = args.x_center + 0.5 * args.scale;
    const double y_min = args.y_center - 0.5 * args.scale / aspect;
    const double y_max = args.y_center + 0.5 * args.scale / aspect;

    std::vector<uint8_t> mask(static_cast<std::size_t>(args.width) * args.height, 0);

    thread_pool pool(static_cast<std::size_t>(args.threads));
    std::latch done(args.height);

    // ----- start compute timer (EXCLUDES I/O) -----
    auto t0 = std::chrono::steady_clock::now();

    // launch one coroutine per row
    std::vector<task> tasks;
    tasks.reserve(args.height);
    for (int y = 0; y < args.height; ++y) {
        tasks.emplace_back(render_row(pool, done, mask, y, args.width, args.height,
                                      x_min, x_max, y_min, y_max, args.max_iter));
    }
    for (auto& t : tasks) t.start();

    done.wait();
    for (auto& t : tasks) t.destroy();

    auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    // ----- file I/O (NOT TIMED) -----
    std::ofstream out("mandelbrot.pbm", std::ios::binary);
    if (!out) { std::cerr << "Failed to open mandelbrot.pbm for writing\n"; return 1; }
    out << "P4\n" << args.width << " " << args.height << "\n";
    const int row_bytes = (args.width + 7) / 8;
    std::vector<unsigned char> rowbuf(static_cast<size_t>(row_bytes));
    for (int y = 0; y < args.height; ++y) {
        std::fill(rowbuf.begin(), rowbuf.end(), 0);
        for (int x = 0; x < args.width; ++x) {
            if (mask[static_cast<size_t>(y)*args.width + x]) {
                const int byte_index = x / 8;
                const int bit_index  = 7 - (x % 8); // MSB first
                rowbuf[byte_index] |= static_cast<unsigned char>(1u << bit_index);
            }
        }
        out.write(reinterpret_cast<const char*>(rowbuf.data()), row_bytes);
    }
    out.close();

    std::cout << "Compute time (no I/O): " << ms << " ms\n"
              << "Wrote mandelbrot.pbm (" << args.width << "x" << args.height
              << ", max_iter=" << args.max_iter << ", threads=" << args.threads << ")\n";
    return 0;
}

