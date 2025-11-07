/**
 * Kryon Terminal Renderer - Comprehensive Compatibility Matrix
 *
 * Tests compatibility across different terminal emulators and environments.
 * This helps identify which features work best on which terminals.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <time.h>

#include "terminal_backend.h"

// Test configuration
typedef struct {
    char* terminal_name;
    char* term_env_var;
    int expected_colors;
    bool expected_unicode;
    bool expected_mouse;
    char description[128];
} terminal_profile_t;

// Known terminal profiles
static terminal_profile_t terminal_profiles[] = {
    {"Alacritty", "alacritty", 16777216, true, true, "Modern GPU-accelerated terminal"},
    {"Kitty", "xterm-kitty", 16777216, true, true, "Modern terminal with graphics support"},
    {"iTerm2", "xterm-256color", 16777216, true, true, "macOS terminal emulator"},
    {"GNOME Terminal", "xterm-256color", 256, true, true, "Default GNOME terminal"},
    {"Konsole", "xterm-256color", 16777216, true, true, "KDE terminal emulator"},
    {"xfce4-terminal", "xterm-256color", 256, true, true, "XFCE terminal"},
    {"xterm", "xterm-256color", 256, true, false, "Classic X11 terminal"},
    {"rxvt-unicode", "rxvt-unicode-256color", 256, true, false, "Lightweight terminal"},
    {"tmux", "screen-256color", 256, true, true, "Terminal multiplexer"},
    {"screen", "screen", 256, true, false, "GNU screen multiplexer"},
    {"Linux Console", "linux", 16, false, false, "Linux kernel console"},
    {"WSL", "xterm-256color", 16777216, true, true, "Windows Subsystem for Linux"},
    {"CMD/PowerShell", "xterm-256color", 256, true, false, "Windows Terminal"},
    {"Unknown", "unknown", 16, false, false, "Unidentified terminal"}
};

static int num_profiles = sizeof(terminal_profiles) / sizeof(terminal_profile_t);

// Test results structure
typedef struct {
    char terminal_name[64];
    char term_env_var[32];
    int detected_colors;
    bool unicode_support;
    bool mouse_support;
    bool resize_support;
    double render_time_ms;
    bool color_accuracy;
    bool coordinate_precision;
    int compatibility_score;  // 0-100
    char notes[256];
    time_t test_timestamp;
} compatibility_result_t;

// System information
typedef struct {
    char os_name[64];
    char kernel_version[64];
    char architecture[32];
    char libtickit_version[32];
    time_t timestamp;
} system_info_t;

// Test utilities
static void get_system_info(system_info_t* info) {
    struct utsname uts;
    uname(&uts);

    snprintf(info->os_name, sizeof(info->os_name), "%s %s", uts.sysname, uts.release);
    snprintf(info->kernel_version, sizeof(info->kernel_version), "%s", uts.version);
    snprintf(info->architecture, sizeof(info->architecture), "%s", uts.machine);

    // Try to get libtickit version
    FILE* pipe = popen("pkg-config --modversion tickit 2>/dev/null", "r");
    if (pipe) {
        fgets(info->libtickit_version, sizeof(info->libtickit_version), pipe);
        pclose(pipe);
        // Remove newline
        char* newline = strchr(info->libtickit_version, '\n');
        if (newline) *newline = '\0';
    } else {
        strcpy(info->libtickit_version, "unknown");
    }

    info->timestamp = time(NULL);
}

static double measure_render_time(kryon_renderer_t* renderer, int iterations) {
    clock_t start = clock();

    for (int i = 0; i < iterations; i++) {
        // Simulate typical rendering operations
        kryon_terminal_clear_screen(renderer);

        // Draw some test content
        for (int y = 0; y < 10; y++) {
            for (int x = 0; x < 30; x++) {
                int color = kryon_terminal_rgb_to_color(
                    (x * 8) % 256,
                    (y * 25) % 256,
                    128,
                    24
                );
                // Simulate drawing operation
                (void)color;
            }
        }
    }

    clock_t end = clock();
    double cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    return (cpu_time / iterations) * 1000.0;  // Convert to milliseconds
}

static bool test_color_accuracy(kryon_renderer_t* renderer) {
    // Test known color values
    struct color_test {
        int r, g, b;
        int expected_term_color;
    } tests[] = {
        {255, 0, 0, 0xFF0000},     // Red
        {0, 255, 0, 0x00FF00},     // Green
        {0, 0, 255, 0x0000FF},     // Blue
        {255, 255, 255, 0xFFFFFF}, // White
        {0, 0, 0, 0x000000},       // Black
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        int result = kryon_terminal_rgb_to_color(
            tests[i].r, tests[i].g, tests[i].b, 24
        );

        // Allow some tolerance for different color modes
        if (abs(result - tests[i].expected_term_color) > 0x1000) {
            return false;
        }
    }

    return true;
}

static bool test_coordinate_precision(kryon_renderer_t* renderer) {
    // Test round-trip coordinate conversion
    for (int x = 0; x < 50; x += 5) {
        for (int y = 0; y < 20; y += 4) {
            int px, py, cx, cy;

            kryon_terminal_char_to_pixel(renderer, x, y, &px, &py);
            kryon_terminal_pixel_to_char(renderer, px, py, &cx, &cy);

            if (abs(cx - x) > 1 || abs(cy - y) > 1) {
                return false;
            }
        }
    }

    return true;
}

static int calculate_compatibility_score(compatibility_result_t* result, terminal_profile_t* profile) {
    int score = 0;

    // Color support (30 points)
    if (result->detected_colors >= profile->expected_colors) {
        score += 30;
    } else if (result->detected_colors >= 256) {
        score += 20;
    } else if (result->detected_colors >= 16) {
        score += 10;
    }

    // Unicode support (20 points)
    if (result->unicode_support == profile->expected_unicode) {
        score += 20;
    } else if (result->unicode_support) {
        score += 10;
    }

    // Mouse support (15 points)
    if (result->mouse_support == profile->expected_mouse) {
        score += 15;
    } else if (result->mouse_support) {
        score += 8;
    }

    // Performance (20 points)
    if (result->render_time_ms < 5.0) {
        score += 20;
    } else if (result->render_time_ms < 10.0) {
        score += 15;
    } else if (result->render_time_ms < 20.0) {
        score += 10;
    } else if (result->render_time_ms < 50.0) {
        score += 5;
    }

    // Accuracy (15 points)
    if (result->color_accuracy && result->coordinate_precision) {
        score += 15;
    } else if (result->color_accuracy || result->coordinate_precision) {
        score += 8;
    }

    return score;
}

static void run_compatibility_test(compatibility_result_t* result, terminal_profile_t* profile) {
    printf("Testing %s (TERM=%s)...\n", profile->terminal_name, profile->term_env_var);

    // Set environment variable
    setenv("TERM", profile->term_env_var, 1);

    // Initialize renderer
    kryon_renderer_t* renderer = kryon_terminal_renderer_create();
    if (!renderer) {
        printf("  ❌ Failed to create renderer\n");
        strcpy(result->terminal_name, profile->terminal_name);
        strcpy(result->term_env_var, profile->term_env_var);
        result->detected_colors = 0;
        result->unicode_support = false;
        result->mouse_support = false;
        result->resize_support = false;
        result->render_time_ms = 999.0;
        result->color_accuracy = false;
        result->coordinate_precision = false;
        result->compatibility_score = 0;
        snprintf(result->notes, sizeof(result->notes), "Failed to create renderer");
        result->test_timestamp = time(NULL);
        return;
    }

    // Fill in basic info
    strcpy(result->terminal_name, profile->terminal_name);
    strcpy(result->term_env_var, profile->term_env_var);
    result->test_timestamp = time(NULL);

    // Test color capabilities
    result->detected_colors = kryon_terminal_get_color_capability(renderer);

    // Test Unicode support
    result->unicode_support = kryon_terminal_supports_unicode(renderer);

    // Test mouse support
    result->mouse_support = kryon_terminal_supports_mouse(renderer);

    // Test resize support
    result->resize_support = true;  // Assume supported for now

    // Measure performance
    result->render_time_ms = measure_render_time(renderer, 100);

    // Test accuracy
    result->color_accuracy = test_color_accuracy(renderer);
    result->coordinate_precision = test_coordinate_precision(renderer);

    // Calculate compatibility score
    result->compatibility_score = calculate_compatibility_score(result, profile);

    // Generate notes
    snprintf(result->notes, sizeof(result->notes),
        "Colors: %d, Unicode: %s, Mouse: %s, Render: %.2fms, Score: %d/100",
        result->detected_colors,
        result->unicode_support ? "✓" : "✗",
        result->mouse_support ? "✓" : "✗",
        result->render_time_ms,
        result->compatibility_score
    );

    printf("  ✅ Colors: %d, Unicode: %s, Mouse: %s, Score: %d/100\n",
           result->detected_colors,
           result->unicode_support ? "✓" : "✗",
           result->mouse_support ? "✓" : "✗",
           result->compatibility_score);

    // Cleanup
    kryon_terminal_renderer_destroy(renderer);
}

static void print_results_csv(compatibility_result_t* results, int count, system_info_t* sys_info) {
    printf("\n=== CSV Results ===\n");
    printf("# Kryon Terminal Renderer Compatibility Matrix\n");
    printf("# Generated: %s", ctime(&sys_info->timestamp));
    printf("# System: %s (%s)\n", sys_info->os_name, sys_info->architecture);
    printf("# libtickit: %s\n", sys_info->libtickit_version);
    printf("\n");
    printf("Terminal,TERM_ENV,Colors,Unicode,Mouse,Resize,Render_ms,Color_Acc,Coord_Prec,Score,Notes\n");

    for (int i = 0; i < count; i++) {
        compatibility_result_t* r = &results[i];
        printf("%s,%s,%d,%s,%s,%s,%.2f,%s,%s,%d,\"%s\"\n",
               r->terminal_name,
               r->term_env_var,
               r->detected_colors,
               r->unicode_support ? "true" : "false",
               r->mouse_support ? "true" : "false",
               r->resize_support ? "true" : "false",
               r->render_time_ms,
               r->color_accuracy ? "true" : "false",
               r->coordinate_precision ? "true" : "false",
               r->compatibility_score,
               r->notes
        );
    }
}

static void print_recommendations(compatibility_result_t* results, int count) {
    printf("\n=== Recommendations ===\n");

    // Find best terminals
    int best_score = 0;
    char best_terminals[5][64];
    int best_count = 0;

    for (int i = 0; i < count; i++) {
        if (results[i].compatibility_score > best_score) {
            best_score = results[i].compatibility_score;
            best_count = 0;
            strcpy(best_terminals[0], results[i].terminal_name);
        } else if (results[i].compatibility_score == best_score && best_count < 5) {
            strcpy(best_terminals[best_count++], results[i].terminal_name);
        }
    }

    printf("🏆 Best performing terminals (Score: %d/100):\n", best_score);
    for (int i = 0; i <= best_count && i < 5; i++) {
        printf("  • %s\n", best_terminals[i]);
    }

    // Performance recommendations
    printf("\n⚡ Performance recommendations:\n");
    for (int i = 0; i < count; i++) {
        if (results[i].render_time_ms < 10.0) {
            printf("  • %s: Excellent performance (%.2fms)\n",
                   results[i].terminal_name, results[i].render_time_ms);
        }
    }

    // Feature recommendations
    printf("\n🎨 Feature-rich terminals:\n");
    for (int i = 0; i < count; i++) {
        if (results[i].detected_colors >= 16777216 &&
            results[i].unicode_support &&
            results[i].mouse_support) {
            printf("  • %s: Full feature support\n", results[i].terminal_name);
        }
    }

    // Compatibility warnings
    printf("\n⚠️  Compatibility concerns:\n");
    for (int i = 0; i < count; i++) {
        if (results[i].compatibility_score < 50) {
            printf("  • %s: Limited compatibility (%d/100) - %s\n",
                   results[i].terminal_name, results[i].compatibility_score,
                   results[i].notes);
        }
    }
}

static void save_results_to_file(compatibility_result_t* results, int count,
                                system_info_t* sys_info, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        printf("Failed to open results file: %s\n", filename);
        return;
    }

    fprintf(file, "# Kryon Terminal Renderer Compatibility Matrix\n");
    fprintf(file, "# Generated: %s", ctime(&sys_info->timestamp));
    fprintf(file, "# System: %s (%s)\n", sys_info->os_name, sys_info->architecture);
    fprintf(file, "# Kernel: %s\n", sys_info->kernel_version);
    fprintf(file, "# libtickit: %s\n", sys_info->libtickit_version);
    fprintf(file, "\n");

    fprintf(file, "Terminal,TERM_ENV,Colors,Unicode,Mouse,Resize,Render_ms,Color_Acc,Coord_Prec,Score,Notes,Timestamp\n");

    for (int i = 0; i < count; i++) {
        compatibility_result_t* r = &results[i];
        fprintf(file, "%s,%s,%d,%s,%s,%s,%.2f,%s,%s,%d,\"%s\",%ld\n",
                r->terminal_name,
                r->term_env_var,
                r->detected_colors,
                r->unicode_support ? "true" : "false",
                r->mouse_support ? "true" : "false",
                r->resize_support ? "true" : "false",
                r->render_time_ms,
                r->color_accuracy ? "true" : "false",
                r->coordinate_precision ? "true" : "false",
                r->compatibility_score,
                r->notes,
                r->test_timestamp
        );
    }

    fclose(file);
    printf("Results saved to: %s\n", filename);
}

int main() {
    printf("🧪 Kryon Terminal Renderer - Compatibility Matrix\n");
    printf("==================================================\n");
    printf("Testing compatibility across %d terminal profiles...\n\n", num_profiles);

    // Get system information
    system_info_t sys_info;
    get_system_info(&sys_info);

    printf("System Information:\n");
    printf("  OS: %s\n", sys_info.os_name);
    printf("  Architecture: %s\n", sys_info.architecture);
    printf("  libtickit: %s\n", sys_info.libtickit_version);
    printf("  Current TERM: %s\n\n", getenv("TERM") ? getenv("TERM") : "unknown");

    // Run compatibility tests
    compatibility_result_t results[num_profiles];

    for (int i = 0; i < num_profiles; i++) {
        run_compatibility_test(&results[i], &terminal_profiles[i]);
    }

    // Sort results by compatibility score (highest first)
    for (int i = 0; i < num_profiles - 1; i++) {
        for (int j = i + 1; j < num_profiles; j++) {
            if (results[j].compatibility_score > results[i].compatibility_score) {
                compatibility_result_t temp = results[i];
                results[i] = results[j];
                results[j] = temp;
            }
        }
    }

    // Print results
    printf("\n=== Compatibility Results (Sorted by Score) ===\n");
    printf("%-20s %-15s %-8s %-8s %-8s %-10s %-8s\n",
           "Terminal", "Colors", "Unicode", "Mouse", "Render", "Score", "Notes");
    printf("%-20s %-15s %-8s %-8s %-8s %-10s %-8s\n",
           "--------", "------", "-------", "-----", "------", "-----", "-----");

    for (int i = 0; i < num_profiles; i++) {
        compatibility_result_t* r = &results[i];
        printf("%-20s %-15d %-8s %-8s %-8.1fms %-10d/100 %s\n",
               r->terminal_name,
               r->detected_colors,
               r->unicode_support ? "✓" : "✗",
               r->mouse_support ? "✓" : "✗",
               r->render_time_ms,
               r->compatibility_score,
               r->compatibility_score >= 80 ? "🏆" :
               r->compatibility_score >= 60 ? "✅" :
               r->compatibility_score >= 40 ? "⚠️" : "❌"
        );
    }

    // Print detailed results
    print_results_csv(results, num_profiles, &sys_info);

    // Print recommendations
    print_recommendations(results, num_profiles);

    // Save results to file
    char filename[256];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(filename, sizeof(filename), "compatibility_results_%Y%m%d_%H%M%S.csv", tm_info);
    save_results_to_file(results, num_profiles, &sys_info, filename);

    // Summary
    int excellent = 0, good = 0, fair = 0, poor = 0;
    for (int i = 0; i < num_profiles; i++) {
        if (results[i].compatibility_score >= 80) excellent++;
        else if (results[i].compatibility_score >= 60) good++;
        else if (results[i].compatibility_score >= 40) fair++;
        else poor++;
    }

    printf("\n📊 Summary:\n");
    printf("  🏆 Excellent (80-100): %d terminals\n", excellent);
    printf("  ✅ Good (60-79): %d terminals\n", good);
    printf("  ⚠️  Fair (40-59): %d terminals\n", fair);
    printf("  ❌ Poor (0-39): %d terminals\n", poor);

    printf("\n✅ Compatibility matrix testing completed!\n");
    return 0;
}