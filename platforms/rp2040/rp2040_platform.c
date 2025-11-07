/*
 * RP2040 Platform Integration Kit
 *
 * Provides RP2040-specific implementations for Kryon framework
 * - Pico SDK integration
 * - Flash-based storage with wear leveling
 * - PIO for custom protocols
 * - Power management and multicore support
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Pico SDK includes
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"

// Include Kryon core header
#include "../../core/include/kryon.h"

// ============================================================================
// RP2040 Flash Storage Implementation
// ============================================================================

#define KRYON_RP2040_STORAGE_FLASH_OFFSET (1024 * 1024)  // 1MB offset
#define KRYON_RP2040_STORAGE_SIZE      (64 * 1024)     // 64KB storage
#define KRYON_RP2040_SECTOR_SIZE       FLASH_SECTOR_SIZE
#define KRYON_RP2040_PAGE_SIZE         FLASH_PAGE_SIZE
#define KRYON_RP2040_MAX_ENTRIES      32
#define KRYON_RP2040_MAX_KEY_LEN      32
#define KRYON_RP2040_MAX_VALUE_LEN    128

typedef struct {
    uint8_t magic;          // Magic byte to identify valid entry
    uint8_t used;            // Whether this entry is in use
    uint16_t version;        // Entry version for wear leveling
    char key[KRYON_RP2040_MAX_KEY_LEN];
    uint16_t value_len;
    uint8_t value[KRYON_RP2040_MAX_VALUE_LEN];
    uint16_t checksum;
    uint16_t reserved;       // Alignment and future use
} kryon_rp2040_entry_t;

#define KRYON_RP2040_ENTRY_MAGIC 0xA5

static bool g_flash_initialized = false;
static uint32_t g_storage_base = 0;

uint16_t kryon_rp2040_calculate_checksum(const uint8_t* data, size_t size) {
    uint16_t checksum = 0xAAAA;
    for (size_t i = 0; i < size; i++) {
        checksum ^= (data[i] << 8) | data[i];
        checksum = (checksum << 1) | (checksum >> 15);
    }
    return checksum;
}

bool kryon_rp2040_init_storage(void) {
    if (g_flash_initialized) {
        return true;
    }

    g_storage_base = XIP_BASE + KRYON_RP2040_STORAGE_FLASH_OFFSET;
    g_flash_initialized = true;

    printf("RP2040 storage initialized at 0x%08lX\n", g_storage_base);
    return true;
}

bool kryon_rp2040_storage_set(const char* key, const void* data, size_t size) {
    if (!g_flash_initialized || !key || !data || size == 0 || size > KRYON_RP2040_MAX_VALUE_LEN) {
        return false;
    }

    if (strlen(key) > KRYON_RP2040_MAX_KEY_LEN - 1) {
        return false;
    }

    // Read current entries to find empty or existing slot
    kryon_rp2040_entry_t entries[KRYON_RP2040_MAX_ENTRIES];
    memcpy(entries, (void*)g_storage_base, sizeof(entries));

    // Find existing entry or empty slot
    int entry_index = -1;
    uint16_t highest_version = 0;

    for (int i = 0; i < KRYON_RP2040_MAX_ENTRIES; i++) {
        if (entries[i].magic != KRYON_RP2040_ENTRY_MAGIC) {
            if (entry_index == -1) entry_index = i;
            continue;
        }

        if (strncmp(entries[i].key, key, KRYON_RP2040_MAX_KEY_LEN) == 0) {
            // Found existing entry
            entry_index = i;
            break;
        }

        if (entries[i].version > highest_version) {
            highest_version = entries[i].version;
        }
    }

    if (entry_index == -1) {
        // Find entry with oldest version (simple wear leveling)
        uint16_t oldest_version = highest_version;
        for (int i = 0; i < KRYON_RP2040_MAX_ENTRIES; i++) {
            if (entries[i].magic == KRYON_RP2040_ENTRY_MAGIC && entries[i].version < oldest_version) {
                oldest_version = entries[i].version;
                entry_index = i;
            }
        }
    }

    if (entry_index == -1) {
        return false; // No available slots
    }

    // Prepare new entry
    kryon_rp2040_entry_t new_entry = {0};
    new_entry.magic = KRYON_RP2040_ENTRY_MAGIC;
    new_entry.used = 1;
    new_entry.version = highest_version + 1;
    strncpy(new_entry.key, key, KRYON_RP2040_MAX_KEY_LEN - 1);
    new_entry.value_len = (uint16_t)size;
    memcpy(new_entry.value, data, size);
    new_entry.checksum = kryon_rp2040_calculate_checksum((uint8_t*)&new_entry, sizeof(new_entry) - sizeof(uint16_t));

    // Update entry in memory
    entries[entry_index] = new_entry;

    // Calculate flash address for the entry
    uint32_t entry_addr = g_storage_base + (entry_index * sizeof(kryon_rp2040_entry_t));

    // Erase the page containing the entry
    uint32_t page_addr = entry_addr & ~(KRYON_RP2040_PAGE_SIZE - 1);

    // Disable interrupts during flash operation
    uint32_t interrupts = save_and_disable_interrupts();

    flash_range_erase(page_addr - XIP_BASE, KRYON_RP2040_PAGE_SIZE);

    // Write the updated entry
    flash_range_program(entry_addr - XIP_BASE, (uint8_t*)&entries[entry_index], sizeof(kryon_rp2040_entry_t));

    // Restore interrupts
    restore_interrupts(interrupts);

    return true;
}

bool kryon_rp2040_storage_get(const char* key, void* buffer, size_t* size) {
    if (!g_flash_initialized || !key || !buffer || !size) {
        return false;
    }

    // Read entries from flash
    kryon_rp2040_entry_t entries[KRYON_RP2040_MAX_ENTRIES];
    memcpy(entries, (void*)g_storage_base, sizeof(entries));

    // Find entry
    for (int i = 0; i < KRYON_RP2040_MAX_ENTRIES; i++) {
        if (entries[i].magic != KRYON_RP2040_ENTRY_MAGIC || !entries[i].used) {
            continue;
        }

        if (strncmp(entries[i].key, key, KRYON_RP2040_MAX_KEY_LEN) == 0) {
            // Verify checksum
            uint16_t calc_checksum = kryon_rp2040_calculate_checksum((uint8_t*)&entries[i], sizeof(entries[i]) - sizeof(uint16_t));
            if (calc_checksum != entries[i].checksum) {
                return false; // Corrupted entry
            }

            if (entries[i].value_len > *size) {
                *size = entries[i].value_len;
                return false; // Buffer too small
            }

            memcpy(buffer, entries[i].value, entries[i].value_len);
            *size = entries[i].value_len;
            return true;
        }
    }

    return false; // Not found
}

bool kryon_rp2040_storage_has(const char* key) {
    if (!g_flash_initialized || !key) {
        return false;
    }

    // Read entries from flash
    kryon_rp2040_entry_t entries[KRYON_RP2040_MAX_ENTRIES];
    memcpy(entries, (void*)g_storage_base, sizeof(entries));

    // Search for entry
    for (int i = 0; i < KRYON_RP2040_MAX_ENTRIES; i++) {
        if (entries[i].magic != KRYON_RP2040_ENTRY_MAGIC || !entries[i].used) {
            continue;
        }

        if (strncmp(entries[i].key, key, KRYON_RP2040_MAX_KEY_LEN) == 0) {
            // Verify checksum
            uint16_t calc_checksum = kryon_rp2040_calculate_checksum((uint8_t*)&entries[i], sizeof(entries[i]) - sizeof(uint16_t));
            return calc_checksum == entries[i].checksum;
        }
    }

    return false;
}

// ============================================================================
// RP2040 Timing and Clock Utilities
// ============================================================================

uint32_t kryon_rp2040_get_timestamp(void) {
    // Return microseconds since boot (more precise than millis)
    return time_us_64() / 1000;
}

void kryon_rp2040_delay_ms(uint32_t ms) {
    sleep_ms(ms);
}

void kryon_rp2040_delay_us(uint32_t us) {
    sleep_us(us);
}

uint32_t kryon_rp2040_get_system_clock_hz(void) {
    return clock_get_hz(clk_sys);
}

bool kryon_rp2040_init_clock(void) {
    // Initialize standard I/O and clock system
    stdio_init_all();

    printf("RP2040 system clock: %lu Hz\n", clock_get_hz(clk_sys));
    return true;
}

// ============================================================================
// RP2040 GPIO and PWM
// ============================================================================

typedef struct {
    uint8_t gpio_pin;
    bool pwm_enabled;
    uint8_t pwm_slice;
    uint8_t pwm_channel;
    uint16_t pwm_wrap;
} kryon_rp2040_gpio_config_t;

#define KRYON_RP2040_MAX_GPIO_CONFIGS 16
static kryon_rp2040_gpio_config_t g_gpio_configs[KRYON_RP2040_MAX_GPIO_CONFIGS];
static uint8_t g_gpio_config_count = 0;

bool kryon_rp2040_gpio_init(uint8_t pin, bool output) {
    gpio_init(pin);
    if (output) {
        gpio_set_dir(pin, GPIO_OUT);
    } else {
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin); // Default pull-up for inputs
    }

    // Store configuration
    if (g_gpio_config_count < KRYON_RP2040_MAX_GPIO_CONFIGS) {
        g_gpio_configs[g_gpio_config_count].gpio_pin = pin;
        g_gpio_configs[g_gpio_config_count].pwm_enabled = false;
        g_gpio_config_count++;
    }

    return true;
}

void kryon_rp2040_gpio_set(uint8_t pin, bool state) {
    gpio_put(pin, state);
}

bool kryon_rp2040_gpio_get(uint8_t pin) {
    return gpio_get(pin);
}

bool kryon_rp2040_gpio_init_pwm(uint8_t pin, uint16_t frequency, uint16_t duty_cycle) {
    // Find free PWM slice
    uint slice_num = pwm_gpio_to_slice_num(pin);
    if (slice_num >= 6) {
        return false; // Invalid PWM pin
    }

    // Configure PWM
    gpio_set_function(pin, GPIO_FUNC_PWM);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_wrap(&config, 255); // 8-bit resolution
    pwm_init(slice_num, &config, true);

    // Set duty cycle
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(pin), (duty_cycle * 255) / 100);

    // Store configuration
    if (g_gpio_config_count < KRYON_RP2040_MAX_GPIO_CONFIGS) {
        g_gpio_configs[g_gpio_config_count].gpio_pin = pin;
        g_gpio_configs[g_gpio_config_count].pwm_enabled = true;
        g_gpio_configs[g_gpio_config_count].pwm_slice = slice_num;
        g_gpio_configs[g_gpio_config_count].pwm_channel = pwm_gpio_to_channel(pin);
        g_gpio_config_count++;
    }

    return true;
}

void kryon_rp2040_pwm_set_duty(uint8_t pin, uint16_t duty_cycle) {
    uint slice_num = pwm_gpio_to_slice_num(pin);
    if (slice_num >= 6) return;

    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(pin), (duty_cycle * 255) / 100);
}

// ============================================================================
// RP2040 ADC
// ============================================================================

bool kryon_rp2040_adc_init(uint8_t adc_pin) {
    if (adc_pin < 26 || adc_pin > 29) {
        return false; // ADC pins are GPIO 26-29
    }

    adc_init();
    adc_gpio_init(adc_pin);
    adc_select_input(adc_pin - 26);

    return true;
}

uint16_t kryon_rp2040_adc_read(uint8_t adc_pin) {
    if (adc_pin < 26 || adc_pin > 29) {
        return 0;
    }

    adc_select_input(adc_pin - 26);
    return adc_read(); // Returns 12-bit value (0-4095)
}

// ============================================================================
// RP2040 Multicore Support
// ============================================================================

typedef struct {
    void (*entry_func)(void);
    bool running;
    uint32_t stack_base;
} kryon_rp2040_core_t;

static kryon_rp2040_core_t g_cores[2] = {0};

bool kryon_rp2040_launch_core1(void (*entry_func)(void), uint32_t stack_size) {
    if (g_cores[1].running) {
        return false; // Core 1 already running
    }

    // Allocate stack for core 1
    uint32_t* core1_stack = malloc(stack_size);
    if (!core1_stack) {
        return false;
    }

    g_cores[1].entry_func = entry_func;
    g_cores[1].running = true;
    g_cores[1].stack_base = (uint32_t)core1_stack + stack_size;

    multicore_launch_core1(entry_func);
    return true;
}

void kryon_rp2040_stop_core1(void) {
    multicore_reset_core1();
    g_cores[1].running = false;
}

bool kryon_rp2040_is_core1_running(void) {
    return g_cores[1].running;
}

// ============================================================================
// RP2040 Power Management
// ============================================================================

typedef enum {
    KRYON_RP2040_POWER_ACTIVE,
    KRYON_RP2040_POWER_SLEEP,
    KRYON_RP2040_POWER_DEEPSLEEP
} kryon_rp2040_power_mode_t;

bool kryon_rp2040_set_power_mode(kryon_rp2040_power_mode_t mode) {
    switch (mode) {
        case KRYON_RP2040_POWER_ACTIVE:
            // Normal operation - wake up all peripherals
            clocks_hw->clk[clk_sys].ctrl |= CLOCKS_CLK_SYS_CTRL_ENABLE_BITS;
            break;

        case KRYON_RP2040_POWER_SLEEP:
            // Light sleep - wake from any interrupt
            // Note: This would require specific wake-up source configuration
            __wfi(); // Wait for interrupt
            break;

        case KRYON_RP2040_POWER_DEEPSLEEP:
            // Deep sleep - requires wake-up from RTC or GPIO
            // Configure wake-up sources before entering deep sleep
            // This is a simplified version
            __wfi();
            break;

        default:
            return false;
    }

    return true;
}

// ============================================================================
// RP2040 Platform Information
// ============================================================================

void kryon_rp2040_print_info(void) {
    printf("=== RP2040 Platform Information ===\n");
    printf("System Clock: %lu Hz\n", clock_get_hz(clk_sys));
    printf("USB Clock: %lu Hz\n", clock_get_hz(clk_usb));
    printf("ADC Clock: %lu Hz\n", clock_get_hz(clk_adc));
    printf("Flash Size: %d KB\n", PICO_FLASH_SIZE_BYTES / 1024);
    printf("SRAM Size: %d KB\n", 264); // RP2040 has 264KB SRAM
    printf("Core Count: 2 (dual core)\n");
    printf("PIO State Machines: 4\n");
    printf("Storage: %d KB @ 0x%08lX\n", KRYON_RP2040_STORAGE_SIZE / 1024, g_storage_base);
    printf("Temperature: %.2f C\n", temperature_sensor_get_temp());
    printf("================================\n");
}

// ============================================================================
// RP2040 Platform Initialization
// ============================================================================

bool kryon_rp2040_platform_init(void) {
    // Initialize basic systems
    stdio_init_all();

    // Initialize clock system
    if (!kryon_rp2040_init_clock()) {
        return false;
    }

    // Initialize storage
    if (!kryon_rp2040_init_storage()) {
        return false;
    }

    // Initialize ADC
    adc_init();

    printf("RP2040 platform initialized\n");
    return true;
}

void kryon_rp2040_platform_shutdown(void) {
    // Stop core 1 if running
    if (kryon_rp2040_is_core1_running()) {
        kryon_rp2040_stop_core1();
    }

    // Enter deep sleep
    kryon_rp2040_set_power_mode(KRYON_RP2040_POWER_DEEPSLEEP);
}