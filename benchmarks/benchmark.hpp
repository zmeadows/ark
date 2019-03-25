#pragma once

#include <chrono>
#include <iostream>
#include <iomanip>
#include <math.h>

using namespace std::chrono;

template <typename BuildFunc, typename IterFunc>
void ecs_bench(const char* label,
               const char* framework,
               size_t entity_count,
               BuildFunc&& build,
               IterFunc&& iterate,
               double relative_precision = 0.15)
{
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    std::cout << "===================================================================" << std::endl;
    std::cout << "ecs benchmark: " << label << std::endl;
    std::cout << "framework: " << framework << std::endl;
    std::cout << "# of entities: " << entity_count << std::endl;

    const auto build_start_time = high_resolution_clock::now();
    auto* world = build(entity_count);
    const auto build_end_time = high_resolution_clock::now();
    const double build_duration = duration_cast<duration<double>>(build_end_time - build_start_time).count();
    std::cout << "world+entity build time: " << build_duration << " (seconds)" << std::endl;

    // first find appropriate benchmark chunk size
    size_t in_chunks_of = 3;
    double chunk_time = 0.0;
    double total_test_time = 0;
    double total_test_iterations = 0.0;
    while (chunk_time < 0.5) {
        in_chunks_of = (size_t) ((double) in_chunks_of * 1.61803398875);
        const auto chunk_start_time = high_resolution_clock::now();
        for (size_t i = 0; i < in_chunks_of; i++) {
            iterate(world);
        }
        const auto chunk_end_time = high_resolution_clock::now();
        chunk_time = duration_cast<duration<double>>(chunk_end_time - chunk_start_time).count();
        total_test_time += (double) chunk_time;
        total_test_iterations += (double) in_chunks_of;
    }

    const size_t bench_chunk_size = in_chunks_of;
    double count = 1.0;
    double mean = total_test_time / total_test_iterations;
    double M2 = mean;

    auto stddev = [&](void) {
        return std::sqrt(M2 / ((double) count * (double) bench_chunk_size));
    };

    // run the iteration for a few seconds without timing
    // this is done to 'normalize' the world for benchmarks that add/remove entities/components
    for (size_t warmup = 0; warmup < 10; warmup++) {
        for (size_t ichunk = 0; ichunk < bench_chunk_size; ichunk++) {
            iterate(world);
        }
    }

    do { // the actual iteration benchmarking
        const auto iter_start_time = high_resolution_clock::now();
        for (size_t ichunk = 0; ichunk < bench_chunk_size; ichunk++) {
            iterate(world);
        }
        const auto iteration_end_time = high_resolution_clock::now();
        const double chunk_duration = duration_cast<duration<double>>(iteration_end_time - iter_start_time).count();

        const double mean_in_chunk = chunk_duration / (double) bench_chunk_size;

        {
            count += 1.0;
            const double delta = mean_in_chunk - mean;
            mean += delta / count;
            const double delta2 = mean_in_chunk - mean;
            M2 += delta * delta2;
        }

        const size_t total_iterations = (size_t) count * in_chunks_of;
        std::cout << '\r' << "iterations completed: " << std::setw(2) << std::setfill('0') << total_iterations << std::flush;
    } while (stddev()/std::abs(mean) > relative_precision);

    delete world;

    std::cout << std::endl;

    static constexpr double frame_time_60fps = 1.0 / 60.0;
    const double overhead_60fps = 100.0 * mean / frame_time_60fps;

    std::cout << "time per system iteration: " << mean
              << "+/- " << stddev() << " (seconds)"
              << std::endl << "                           [ " << overhead_60fps << "% of frame @ 60fps]"
              << std::endl;

    std::cout << "===================================================================" << std::endl;
    std::cout << std::endl;
}

