/**
 * Kryon Terminal Renderer - Stress Test Suite
 *
 * High-load stress testing to ensure the terminal backend remains stable
 * under extreme conditions and usage patterns.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/resource.h>
#include <pthread.h>
#include <assert.h>

#include "terminal_backend.h"

// Stress test configuration
#define STRESS_ITERATIONS 100000
#define MEMORY_CYCLES 10000
#define CONCURRENT_THREADS 8
#define LARGE_CONTENT_SIZE 10000
#define STRESS_TIMEOUT 300  // 5 minutes max

// Test statistics
typedef struct {
    int iterations_completed;
    int memory_allocations;
    double total_time;
    double max_memory_mb;
    int errors_encountered;
    bool test_passed;
} stress_stats_t;

static stress_stats_t g_stats = {0};
static volatile bool g_stop_stress_test = false;

// Signal handler for timeout
void timeout_handler(int sig) {
    (void)sig;
    printf("\n⏰ Stress test timeout reached!\n");
    g_stop_stress_test = true;
}

// Memory usage monitoring
static double get_memory_usage_mb() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss / 1024.0;  // Convert to MB on Linux
}

// Thread data for concurrent testing
typedef struct {
    int thread_id;
    int iterations;
    stress_stats_t* stats;
    kryon_renderer_t* renderer;
} thread_data_t;

// Stress test functions
static bool stress_test_renderer_creation_destruction(int cycles) {
    printf("Stress testing renderer creation/destruction (%d cycles)...\n", cycles);

    clock_t start = clock();
    int successful_cycles = 0;

    for (int i = 0; i < cycles && !g_stop_stress_test; i++) {
        kryon_renderer_t* renderer = kryon_terminal_renderer_create();
        if (renderer) {
            // Perform some operations
            uint16_t width, height;
            kryon_terminal_get_size(renderer, &width, &height);

            // Test color conversions
            for (int j = 0; j < 100; j++) {
                int color = kryon_terminal_rgb_to_color(j % 256, (j*2) % 256, (j*3) % 256, 24);
                (void)color;
            }

            kryon_terminal_renderer_destroy(renderer);
            successful_cycles++;
        } else {
            g_stats.errors_encountered++;
        }

        if (i % 1000 == 0) {
            printf("  Progress: %d/%d cycles (%.1f%%)\r", i, cycles, (float)i/cycles * 100);
            fflush(stdout);

            // Update memory usage
            double current_memory = get_memory_usage_mb();
            if (current_memory > g_stats.max_memory_mb) {
                g_stats.max_memory_mb = current_memory;
            }
        }
    }

    clock_t end = clock();
    g_stats.total_time += ((double)(end - start)) / CLOCKS_PER_SEC;
    g_stats.iterations_completed += successful_cycles;
    g_stats.memory_allocations += cycles;

    printf("\n  ✅ Completed %d/%d successful cycles (%.2f%% success rate)\n",
           successful_cycles, cycles, (float)successful_cycles/cycles * 100);

    return successful_cycles >= cycles * 0.95;  // 95% success rate required
}

static bool stress_test_color_conversions(int iterations) {
    printf("Stress testing color conversions (%d iterations)...\n", iterations);

    clock_t start = clock();
    int successful_conversions = 0;

    for (int i = 0; i < iterations && !g_stop_stress_test; i++) {
        // Test various color modes and values
        int r = i % 256;
        int g = (i * 2) % 256;
        int b = (i * 3) % 256;

        // 24-bit color
        int color24 = kryon_terminal_rgb_to_color(r, g, b, 24);
        if (color24 >= 0) successful_conversions++;

        // 256-color
        int color256 = kryon_terminal_rgb_to_color(r, g, b, 8);
        if (color256 >= 0 && color256 < 256) successful_conversions++;

        // 16-color
        int color16 = kryon_terminal_rgb_to_color(r, g, b, 4);
        if (color16 >= 0 && color16 < 16) successful_conversions++;

        // Test Kryon color conversion
        uint32_t kryon_color = kryon_color_rgba(r, g, b, i % 256);
        int term_color = kryon_terminal_kryon_color_to_color(kryon_color, 24);
        if (term_color >= 0) successful_conversions++;

        if (i % 10000 == 0) {
            printf("  Progress: %d/%d conversions\r", i, iterations);
            fflush(stdout);
        }
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    double ops_per_sec = (iterations * 4) / time_taken;  // 4 conversions per iteration

    printf("\n  ✅ Color conversion performance: %.0f ops/sec\n", ops_per_sec);
    printf("  ✅ Successful conversions: %d/%d (%.2f%%)\n",
           successful_conversions, iterations * 4, (float)successful_conversions/(iterations * 4) * 100);

    g_stats.total_time += time_taken;
    g_stats.iterations_completed += iterations;

    return ops_per_sec > 100000;  // Should handle at least 100k ops/sec
}

static bool stress_test_coordinate_conversions(int iterations) {
    printf("Stress testing coordinate conversions (%d iterations)...\n", iterations);

    clock_t start = clock();
    int successful_conversions = 0;

    for (int i = 0; i < iterations && !g_stop_stress_test; i++) {
        int x = i % 200;
        int y = i % 100;

        // Test char to pixel conversion
        int px, py;
        kryon_terminal_char_to_pixel(NULL, x, y, &px, &py);

        // Test pixel to char conversion
        int cx, cy;
        kryon_terminal_pixel_to_char(NULL, px, py, &cx, &cy);

        // Verify round-trip accuracy
        if (abs(cx - x) <= 1 && abs(cy - y) <= 1) {
            successful_conversions++;
        } else {
            g_stats.errors_encountered++;
        }

        if (i % 10000 == 0) {
            printf("  Progress: %d/%d conversions\r", i, iterations);
            fflush(stdout);
        }
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
    double ops_per_sec = iterations / time_taken;

    printf("\n  ✅ Coordinate conversion performance: %.0f ops/sec\n", ops_per_sec);
    printf("  ✅ Successful conversions: %d/%d (%.2f%%)\n",
           successful_conversions, iterations, (float)successful_conversions/iterations * 100);

    g_stats.total_time += time_taken;
    g_stats.iterations_completed += iterations;

    return (float)successful_conversions/iterations > 0.99;  // 99% accuracy required
}

static void* concurrent_test_thread(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    stress_stats_t local_stats = {0};

    printf("  Thread %d: Starting stress test (%d iterations)...\n",
           data->thread_id, data->iterations);

    clock_t thread_start = clock();

    for (int i = 0; i < data->iterations && !g_stop_stress_test; i++) {
        // Create renderer
        kryon_renderer_t* renderer = kryon_terminal_renderer_create();
        if (renderer) {
            // Perform operations
            uint16_t width, height;
            kryon_terminal_get_size(renderer, &width, &height);

            // Test operations
            kryon_terminal_clear_screen(renderer);
            kryon_terminal_set_cursor_visible(renderer, false);
            kryon_terminal_set_cursor_visible(renderer, true);

            // Color operations
            for (int j = 0; j < 10; j++) {
                int color = kryon_terminal_rgb_to_color(
                    (i + j) % 256,
                    (i * 2 + j) % 256,
                    (i * 3 + j) % 256,
                    24
                );
                (void)color;
            }

            kryon_terminal_renderer_destroy(renderer);
            local_stats.iterations_completed++;
        } else {
            local_stats.errors_encountered++;
        }

        // Small delay to simulate real usage
        usleep(1000);  // 1ms
    }

    clock_t thread_end = clock();
    local_stats.total_time = ((double)(thread_end - thread_start)) / CLOCKS_PER_SEC;

    // Update global stats
    data->stats->iterations_completed += local_stats.iterations_completed;
    data->stats->errors_encountered += local_stats.errors_encountered;

    printf("  Thread %d: Completed %d iterations in %.2fs\n",
           data->thread_id, local_stats.iterations_completed, local_stats.total_time);

    return NULL;
}

static bool stress_test_concurrent_access(int num_threads, int iterations_per_thread) {
    printf("Stress testing concurrent access (%d threads, %d iterations each)...\n",
           num_threads, iterations_per_thread);

    pthread_t threads[num_threads];
    thread_data_t thread_data[num_threads];
    stress_stats_t concurrent_stats = {0};

    // Create threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].iterations = iterations_per_thread;
        thread_data[i].stats = &concurrent_stats;
        thread_data[i].renderer = NULL;

        int result = pthread_create(&threads[i], NULL, concurrent_test_thread, &thread_data[i]);
        if (result != 0) {
            printf("  ❌ Failed to create thread %d\n", i);
            g_stats.errors_encountered++;
            return false;
        }
    }

    // Wait for threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Update global stats
    g_stats.iterations_completed += concurrent_stats.iterations_completed;
    g_stats.errors_encountered += concurrent_stats.errors_encountered;

    printf("  ✅ Concurrent test completed: %d total iterations\n",
           concurrent_stats.iterations_completed);
    printf("  ✅ Errors encountered: %d\n", concurrent_stats.errors_encountered);

    return concurrent_stats.errors_encountered < (iterations_per_thread * num_threads) * 0.05;  // <5% errors
}

static bool stress_test_memory_usage(int cycles) {
    printf("Stress testing memory usage (%d allocation cycles)...\n", cycles);

    clock_t start = clock();
    double initial_memory = get_memory_usage_mb();

    // Allocate many renderers simultaneously
    kryon_renderer_t* renderers[100];
    int allocated = 0;

    for (int cycle = 0; cycle < cycles && !g_stop_stress_test; cycle++) {
        // Allocate renderers
        for (int i = 0; i < 100; i++) {
            renderers[i] = kryon_terminal_renderer_create();
            if (renderers[i]) {
                allocated++;

                // Perform some operations
                uint16_t width, height;
                kryon_terminal_get_size(renderers[i], &width, &height);

                // Generate some load
                for (int j = 0; j < 50; j++) {
                    int color = kryon_terminal_rgb_to_color(
                        (cycle + i + j) % 256,
                        (cycle * 2 + i + j) % 256,
                        (cycle * 3 + i + j) % 256,
                        24
                    );
                    (void)color;
                }
            }
        }

        // Check memory usage
        double current_memory = get_memory_usage_mb();
        if (current_memory > g_stats.max_memory_mb) {
            g_stats.max_memory_mb = current_memory;
        }

        // Clean up half the renderers
        for (int i = 0; i < 50; i++) {
            if (renderers[i]) {
                kryon_terminal_renderer_destroy(renderers[i]);
                renderers[i] = NULL;
            }
        }

        // Clean up remaining renderers
        for (int i = 50; i < 100; i++) {
            if (renderers[i]) {
                kryon_terminal_renderer_destroy(renderers[i]);
                renderers[i] = NULL;
            }
        }

        if (cycle % 100 == 0) {
            printf("  Progress: %d/%d cycles (Memory: %.1f MB)\r",
                   cycle, cycles, current_memory);
            fflush(stdout);
        }
    }

    clock_t end = clock();
    double final_memory = get_memory_usage_mb();
    double memory_leak = final_memory - initial_memory;

    printf("\n  ✅ Memory stress test completed\n");
    printf("  ✅ Initial memory: %.1f MB\n", initial_memory);
    printf("  ✅ Final memory: %.1f MB\n", final_memory);
    printf("  ✅ Memory leak: %.1f MB\n", memory_leak);
    printf("  ✅ Peak memory usage: %.1f MB\n", g_stats.max_memory_mb);

    g_stats.total_time += ((double)(end - start)) / CLOCKS_PER_SEC;
    g_stats.memory_allocations += allocated;

    return memory_leak < 10.0;  // Less than 10MB leak acceptable
}

static bool stress_test_large_content() {
    printf("Stress testing large content rendering...\n");

    kryon_renderer_t* renderer = kryon_terminal_renderer_create();
    if (!renderer) {
        printf("  ❌ Failed to create renderer\n");
        return false;
    }

    clock_t start = clock();
    bool success = true;

    // Test large text rendering simulation
    for (int i = 0; i < 1000 && !g_stop_stress_test; i++) {
        // Simulate large content operations
        for (int j = 0; j < 100; j++) {
            // Color operations for large content
            int color = kryon_terminal_rgb_to_color(
                (i + j) % 256,
                (i * 2 + j) % 256,
                (i * 3 + j) % 256,
                24
            );

            // Coordinate operations for large content
            int x = j % 200;
            int y = (i + j) % 100;
            int px, py;
            kryon_terminal_char_to_pixel(renderer, x, y, &px, &py);

            // More intensive operations
            for (int k = 0; k < 10; k++) {
                uint32_t kryon_color = kryon_color_rgba(
                    (i + j + k) % 256,
                    (i * 2 + j + k) % 256,
                    (i * 3 + j + k) % 256,
                    255
                );
                int term_color = kryon_terminal_kryon_color_to_color(kryon_color, 24);
                (void)term_color;
            }
        }

        if (i % 100 == 0) {
            printf("  Progress: %d/1000 large content operations\r", i);
            fflush(stdout);
        }
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("\n  ✅ Large content test completed in %.2fs\n", time_taken);
    printf("  ✅ Performance: %.0f operations/sec\n", (1000.0 * 100.0 * 10.0) / time_taken);

    kryon_terminal_renderer_destroy(renderer);
    g_stats.iterations_completed += 1000;
    g_stats.total_time += time_taken;

    return time_taken < 30.0;  // Should complete within 30 seconds
}

// Main stress test runner
int main() {
    printf("🔥 Kryon Terminal Renderer - Stress Test Suite\n");
    printf("==============================================\n");
    printf("High-load testing to ensure stability under extreme conditions\n\n");

    // Set up timeout handler
    signal(SIGALRM, timeout_handler);
    alarm(STRESS_TIMEOUT);

    // Initialize statistics
    memset(&g_stats, 0, sizeof(g_stats));
    g_stats.max_memory_mb = get_memory_usage_mb();

    printf("Initial memory usage: %.1f MB\n", g_stats.max_memory_mb);
    printf("Test timeout: %d seconds\n\n", STRESS_TIMEOUT);

    bool all_tests_passed = true;

    // Run stress tests
    printf("Starting stress tests...\n\n");

    if (!stress_test_renderer_creation_destruction(MEMORY_CYCLES)) {
        printf("❌ Renderer creation/destruction stress test failed\n");
        all_tests_passed = false;
    }
    printf("\n");

    if (!stress_test_color_conversions(STRESS_ITERATIONS)) {
        printf("❌ Color conversion stress test failed\n");
        all_tests_passed = false;
    }
    printf("\n");

    if (!stress_test_coordinate_conversions(STRESS_ITERATIONS)) {
        printf("❌ Coordinate conversion stress test failed\n");
        all_tests_passed = false;
    }
    printf("\n");

    if (!stress_test_memory_usage(MEMORY_CYCLES / 10)) {
        printf("❌ Memory usage stress test failed\n");
        all_tests_passed = false;
    }
    printf("\n");

    if (!stress_test_concurrent_access(CONCURRENT_THREADS, MEMORY_CYCLES / 10)) {
        printf("❌ Concurrent access stress test failed\n");
        all_tests_passed = false;
    }
    printf("\n");

    if (!stress_test_large_content()) {
        printf("❌ Large content stress test failed\n");
        all_tests_passed = false;
    }
    printf("\n");

    // Cancel timeout
    alarm(0);

    // Print final statistics
    printf("📊 Stress Test Results Summary\n");
    printf("============================\n");
    printf("Total iterations completed: %d\n", g_stats.iterations_completed);
    printf("Total memory allocations: %d\n", g_stats.memory_allocations);
    printf("Total execution time: %.2f seconds\n", g_stats.total_time);
    printf("Average performance: %.0f operations/sec\n",
           g_stats.iterations_completed / g_stats.total_time);
    printf("Peak memory usage: %.1f MB\n", g_stats.max_memory_mb);
    printf("Errors encountered: %d\n", g_stats.errors_encountered);
    printf("Error rate: %.4f%%\n",
           (float)g_stats.errors_encountered / (g_stats.iterations_completed + g_stats.errors_encountered) * 100);

    if (g_stop_stress_test) {
        printf("⚠️  Test was terminated due to timeout\n");
        all_tests_passed = false;
    }

    // Performance analysis
    printf("\n🎯 Performance Analysis:\n");
    if (g_stats.total_time > 0) {
        printf("  • Throughput: %.0f ops/sec\n", g_stats.iterations_completed / g_stats.total_time);
        printf("  • Memory efficiency: %.2f ops/MB\n",
               g_stats.iterations_completed / g_stats.max_memory_mb);
        printf("  • Error rate: %.4f%%\n",
               (float)g_stats.errors_encountered / (g_stats.iterations_completed + g_stats.errors_encountered) * 100);
    }

    // Recommendations
    printf("\n💡 Recommendations:\n");
    if (g_stats.errors_encountered == 0) {
        printf("  ✅ Excellent stability - no errors detected\n");
    } else if (g_stats.errors_encountered < 100) {
        printf("  ✅ Good stability - minimal errors detected\n");
    } else {
        printf("  ⚠️  Review needed - significant errors detected\n");
    }

    if (g_stats.max_memory_mb < 50.0) {
        printf("  ✅ Excellent memory efficiency\n");
    } else if (g_stats.max_memory_mb < 100.0) {
        printf("  ✅ Acceptable memory usage\n");
    } else {
        printf("  ⚠️  High memory usage detected\n");
    }

    if ((g_stats.iterations_completed / g_stats.total_time) > 50000) {
        printf("  ✅ Excellent performance\n");
    } else if ((g_stats.iterations_completed / g_stats.total_time) > 20000) {
        printf("  ✅ Acceptable performance\n");
    } else {
        printf("  ⚠️  Performance could be improved\n");
    }

    // Final verdict
    printf("\n🏆 Stress Test Verdict:\n");
    if (all_tests_passed && g_stats.errors_encountered == 0 && !g_stop_stress_test) {
        printf("🎉 PASSED - Terminal backend is production-ready!\n");
        printf("   The implementation successfully handled all stress tests without errors.\n");
        return 0;
    } else if (all_tests_passed && g_stats.errors_encountered < 100) {
        printf("✅ PASSED - Terminal backend is production-ready with minor caveats.\n");
        printf("   The implementation handled stress tests well with minimal errors.\n");
        return 0;
    } else {
        printf("❌ FAILED - Terminal backend needs attention before production use.\n");
        printf("   Some stress tests failed or significant errors were detected.\n");
        return 1;
    }
}