#pragma once

#include "ark/ark.hpp"

#include <chrono>
#include <iomanip>
#include <math.h>
#include <random>

using namespace std::chrono;

namespace ark {

namespace bench {

struct Velocity {
    float x = 0;
    float y = 0;
    using Storage = BucketArrayStorage<Velocity, 2000>;
};


struct Position {
    float x = 0;
    float y = 0;
    using Storage = BucketArrayStorage<Position, 2000>;

    inline void advance(float dt, const Velocity& v) {
        x += dt * v.x;
        y += dt * v.y;
    }
};

struct RotationalVelocity {
    float dtheta = 0;
    using Storage = BucketArrayStorage<RotationalVelocity, 2000>;
};

struct Angle {
    float theta = 0;
    using Storage = BucketArrayStorage<Angle, 2000>;

    inline void advance(float dt, const RotationalVelocity& v) {
        theta += dt * v.dtheta;
    }
};

template <size_t N>
std::array<Velocity, N> build_random_velocities(void) {
    std::array<Velocity, N> result;
    std::default_random_engine e;
    std::uniform_real_distribution<> dis(-1.f, 1.f);

    for (size_t i = 0; i < N; i++) {
        Velocity v;
        v.x = dis(e);
        v.y = dis(e);
        result[i] = v;
    }

    return result;
}

void start(const char* label, size_t num_entities) {
    std::cout << "===================================================================" << std::endl;
    std::cout << "ark benchmark: " << label << std::endl;
    std::cout << "# of entities: " << num_entities << std::endl;
}

void end(void) {
    std::cout << "===================================================================" << std::endl;
    std::cout << std::endl;
}

struct BenchmarkResult {
    double mean = 0;
    double std_dev = 0;
    double count = 0;

    void print(const char* prefix) {
        const double frame_percent = 100.0 * mean / 0.016666666;
        std::cout << prefix << ": "
                  << mean << "+/- "
                  << std_dev << " (seconds)"
                  << " [" << frame_percent << "% of 60fps frame]"
                  << std::endl;


    }
};

class ArkBench {
    double count = 0.0;
    double mean = 0.0;
    double M2 = 0.0;

    void include_time(double new_time) {
        count += 1.0;
        const double delta = new_time - mean;
        mean += delta / count;
        const double delta2 = new_time - mean;
        M2 += delta * delta2;
    }

public:

    template <typename Callable>
    BenchmarkResult bench(Callable&& f, size_t in_chunks_of) {
        do {
            const auto start_time = high_resolution_clock::now();
            for (size_t ichunk = 0; ichunk < in_chunks_of; ichunk++) {
                f();
            }
            const auto elapsed = high_resolution_clock::now() - start_time;
            const double chunk_duration = duration_cast<duration<double>>(elapsed).count();
            if (count == 0.0) {
                mean = chunk_duration / (double) in_chunks_of;
                count = 1.0;
                M2 = mean;
            } else {
                include_time(chunk_duration / (double) in_chunks_of);
            }

            std::cout << '\r' << std::setw(2) << std::setfill('0') << (size_t) count * in_chunks_of << " iterations completed" << std::flush;

        } while (std::sqrt(M2 / ((double) count * (double) in_chunks_of))/std::abs(mean) > 0.16);

        std::cout << std::endl;

        BenchmarkResult result;
        result.mean = mean;
        result.std_dev = std::sqrt(M2 / ((double) count * (double) in_chunks_of));
        result.count = count * in_chunks_of;
        return result;
    }
};

}

template <typename Callable>
bench::BenchmarkResult benchmark(Callable&& f, size_t in_chunks_of = 10) {
    bench::ArkBench b;
    return b.bench(std::forward<Callable>(f), in_chunks_of);
}

}
