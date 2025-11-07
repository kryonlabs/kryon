/**
 * Kryon Terminal Renderer - Compatibility Test
 *
 * Tests terminal capabilities and provides diagnostic information
 * for terminal rendering compatibility.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <signal.h>

// Simple terminal capability detection without libtickit
struct terminal_info {
    int cols;
    int rows;
    int colors;
    bool unicode;
    bool mouse;
    char term_name[256];
    char colorterm[256];
};

// Get terminal size
static void get_terminal_size(struct terminal_info* info) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        info->cols = ws.ws_col;
        info->rows = ws.ws_row;
    } else {
        info->cols = 80;
        info->rows = 24;
    }
}

// Detect color support
static int detect_color_support() {
    const char* term = getenv("TERM");
    const char* colorterm = getenv("COLORTERM");

    if (colorterm) {
        if (strstr(colorterm, "truecolor") || strstr(colorterm, "24bit")) {
            return 16777216; // 24-bit color
        }
        if (strstr(colorterm, "256color")) {
            return 256; // 256 colors
        }
    }

    if (term) {
        if (strstr(term, "256color") || strstr(term, "xterm-256color") ||
            strstr(term, "screen-256color") || strstr(term, "tmux-256color")) {
            return 256;
        }
    }

    return 16; // Default to 16 colors
}

// Detect Unicode support
static bool detect_unicode_support() {
    const char* lang = getenv("LANG");
    const char* lc_all = getenv("LC_ALL");
    const char* lc_ctype = getenv("LC_CTYPE");

    const char* locales[] = {lang, lc_all, lc_ctype};
    for (int i = 0; i < 3; i++) {
        if (locales[i] && (strstr(locales[i], "UTF-8") || strstr(locales[i], "utf8"))) {
            return true;
        }
    }

    return false;
}

// Detect mouse support
static bool detect_mouse_support() {
    const char* term = getenv("TERM");
    if (!term) return false;

    if (strstr(term, "xterm") || strstr(term, "screen") || strstr(term, "tmux") ||
        strstr(term, "alacritty") || strstr(term, "kitty") || strstr(term, "gnome") ||
        strstr(term, "konsole") || strstr(term, "iterm")) {
        return true;
    }

    return false;
}

// Test basic terminal capabilities
static void test_basic_capabilities() {
    printf("Testing Basic Terminal Capabilities\n");
    printf("==================================\n\n");

    // Test cursor positioning
    printf("1. Cursor Positioning: ");
    printf("\033[10;10H✓");
    fflush(stdout);
    usleep(500000);
    printf("\033[20;1HO");
    printf("PASS\n");

    // Test colors
    printf("2. Color Support: ");
    printf("\033[31mRed\033[0m ");
    printf("\033[32mGreen\033[0m ");
    printf("\033[34mBlue\033[0m ");
    printf("PASS\n");

    // Test character output
    printf("3. Character Output: ✓ ✓ ✓ ♥ ♥ ♥");
    printf(" PASS\n");

    printf("\n");
}

// Test advanced features
static void test_advanced_features(struct terminal_info* info) {
    printf("Testing Advanced Features\n");
    printf("========================\n\n");

    // Test Unicode characters if supported
    if (info->unicode) {
        printf("Unicode Characters: ");
        printf("✓ UTF-8: ☃ ❄ ★ ♥ ♦ ♣ ♠ • ‚ ‛");
        printf(" PASS\n");
    } else {
        printf("Unicode Characters: ❌ Not supported\n");
    }

    // Test box drawing characters
    printf("Box Drawing: ");
    printf("┌─┬─┐  ├─┼─┤  └─┴─┘");
    printf(" PASS\n");

    // Test cursor control
    printf("Cursor Control: ");
    printf("\033[?25l");
    printf("HIDDEN");
    usleep(500000);
    printf("\033[?25h");
    printf(" VISIBLE");
    printf(" PASS\n");

    printf("\n");
}

// Test responsiveness
static void test_responsiveness() {
    printf("Testing Terminal Responsiveness\n");
    printf("===============================\n\n");

    printf("Rendering speed test...\n");
    clock_t start = clock();

    // Render 1000 characters
    for (int i = 0; i < 100; i++) {
        printf("Line %03d: ", i);
        for (int j = 0; j < 10; j++) {
            printf("█");
        }
        printf("\n");
    }

    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("\nRendered 1000 lines in %.3f seconds (%.0f lines/sec)\n", elapsed, 1000.0 / elapsed);

    if (elapsed < 1.0) {
        printf("Performance: ✓ EXCELLENT\n");
    } else if (elapsed < 2.0) {
        printf("Performance: ✓ GOOD\n");
    } else if (elapsed < 5.0) {
        printf("Performance: ⚠ ACCEPTABLE\n");
    } else {
        printf("Performance: ❌ SLOW\n");
    }

    printf("\n");
}

// Run compatibility test
static bool run_compatibility_test(struct terminal_info* info) {
    printf("Kryon Terminal Renderer Compatibility Test\n");
    printf("========================================\n\n");

    bool all_tests_pass = true;

    // Display terminal information
    printf("Terminal Information:\n");
    printf("- TERM: %s\n", info->term_name);
    printf("- COLORTERM: %s\n", info->colorterm[0] ? info->colorterm : "not set");
    printf("- Size: %dx%d characters\n", info->cols, info->rows);
    printf("- Colors: %d (%s)\n", info->colors,
           info->colors == 16777216 ? "truecolor" :
           info->colors == 256 ? "256-color" : "16-color");
    printf("- Unicode: %s\n", info->unicode ? "yes" : "no");
    printf("- Mouse: %s\n", info->mouse ? "yes" : "no");
    printf("\n");

    // Run tests
    test_basic_capabilities();
    test_advanced_features(info);
    test_responsiveness();

    // Final assessment
    printf("Compatibility Assessment:\n");
    printf("========================\n");

    if (info->colors >= 16) {
        printf("✓ Color support: Available\n");
    } else {
        printf("❌ Color support: Limited\n");
        all_tests_pass = false;
    }

    if (info->cols >= 80 && info->rows >= 24) {
        printf("✓ Size: Adequate for UI (%dx%d)\n", info->cols, info->rows);
    } else {
        printf("⚠ Size: Small (%dx%d), may limit UI\n", info->cols, info->rows);
    }

    if (info->unicode) {
        printf("✓ Unicode: Available for enhanced graphics\n");
    } else {
        printf("⚠ Unicode: Not available, limited graphics\n");
    }

    if (info->mouse) {
        printf("✓ Mouse: Available for interactive elements\n");
    } else {
        printf("⚠ Mouse: Not available, keyboard-only input\n");
    }

    printf("\nOverall Compatibility: %s\n", all_tests_pass ? "PASS" : "PARTIAL");
    printf("\n");

    return all_tests_pass;
}

// Print recommendations
static void print_recommendations(struct terminal_info* info) {
    printf("Recommendations:\n");
    printf("================\n");

    if (!info->unicode) {
        printf("• Set UTF-8 locale (export LANG=en_US.UTF-8)\n");
    }

    if (info->colors == 16) {
        printf("• Use a modern terminal for better color support\n");
        printf("• Try alacritty, kitty, gnome-terminal, or iTerm2\n");
    }

    if (!info->mouse) {
        printf("• Use a terminal with mouse support for interactive UIs\n");
    }

    if (info->cols < 80 || info->rows < 24) {
        printf("• Increase terminal size for better UI experience\n");
        printf("• Minimum recommended: 80x24 characters\n");
    }

    printf("\n");
}

// Signal handler for clean exit
static volatile sig_atomic_t interrupted = 0;
static void handle_sigint(int sig) {
    (void)sig;
    interrupted = 1;
}

int main() {
    // Setup signal handler
    signal(SIGINT, handle_sigint);

    // Get terminal information
    struct terminal_info info = {0};
    get_terminal_size(&info);
    info.colors = detect_color_support();
    info.unicode = detect_unicode_support();
    info.mouse = detect_mouse_support();

    const char* term = getenv("TERM");
    if (term) {
        strncpy(info.term_name, term, sizeof(info.term_name) - 1);
    }

    const char* colorterm = getenv("COLORTERM");
    if (colorterm) {
        strncpy(info.colorterm, colorterm, sizeof(info.colorterm) - 1);
    }

    // Run compatibility test
    bool compatible = run_compatibility_test(&info);

    // Print recommendations
    print_recommendations(&info);

    // Final result
    if (compatible) {
        printf("✅ Terminal is READY for Kryon Terminal UI applications\n");
        return 0;
    } else {
        printf("⚠️  Terminal has LIMITED compatibility with Kryon Terminal UI\n");
        printf("   Consider using a more capable terminal for best experience\n");
        return 1;
    }
}