/*
 * RP2040 Platform Integration Kit - Header
 *
 * Public interface for RP2040-specific Kryon functionality
 */

#ifndef KRYON_RP2040_PLATFORM_H
#define KRYON_RP2040_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Pico SDK includes (conditional)
#ifdef KRYON_RP2040_PICO_SDK_AVAILABLE
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"
#else
// Fallback declarations for compilation without Pico SDK
typedef void* mutex_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Storage Implementation
// ============================================================================

bool kryon_rp2040_init_storage(void);

bool kryon_rp2040_storage_set(const char* key, const void* data, size_t size);
bool kryon_rp2040_storage_get(const char* key, void* buffer, size_t* size);
bool kryon_rp2040_storage_has(const char* key);

// Storage limits
#define KRYON_RP2040_MAX_KEY_LEN      32
#define KRYON_RP2040_MAX_VALUE_LEN    128
#define KRYON_RP2040_MAX_ENTRIES      32

// ============================================================================
// Timing and Clock Utilities
// ============================================================================

uint32_t kryon_rp2040_get_timestamp(void);
void kryon_rp2040_delay_ms(uint32_t ms);
void kryon_rp2040_delay_us(uint32_t us);
uint32_t kryon_rp2040_get_system_clock_hz(void);
bool kryon_rp2040_init_clock(void);

// ============================================================================
// GPIO and PWM
// ============================================================================

bool kryon_rp2040_gpio_init(uint8_t pin, bool output);
void kryon_rp2040_gpio_set(uint8_t pin, bool state);
bool kryon_rp2040_gpio_get(uint8_t pin);

bool kryon_rp2040_gpio_init_pwm(uint8_t pin, uint16_t frequency, uint16_t duty_cycle);
void kryon_rp2040_pwm_set_duty(uint8_t pin, uint16_t duty_cycle);

// GPIO limits
#define KRYON_RP2040_MAX_GPIO_PIN 28

// ============================================================================
// ADC
// ============================================================================

bool kryon_rp2040_adc_init(uint8_t adc_pin);
uint16_t kryon_rp2040_adc_read(uint8_t adc_pin);

// ADC pins are GPIO 26-29
#define KRYON_RP2040_ADC_PIN_MIN 26
#define KRYON_RP2040_ADC_PIN_MAX 29

// ============================================================================
// Multicore Support
// ============================================================================

bool kryon_rp2040_launch_core1(void (*entry_func)(void), uint32_t stack_size);
void kryon_rp2040_stop_core1(void);
bool kryon_rp2040_is_core1_running(void);

// ============================================================================
// Power Management
// ============================================================================

typedef enum {
    KRYON_RP2040_POWER_ACTIVE,
    KRYON_RP2040_POWER_SLEEP,
    KRYON_RP2040_POWER_DEEPSLEEP
} kryon_rp2040_power_mode_t;

bool kryon_rp2040_set_power_mode(kryon_rp2040_power_mode_t mode);

// ============================================================================
// Platform Information
// ============================================================================

void kryon_rp2040_print_info(void);

// Platform constants
#define KRYON_RP2040_CORE_COUNT     2
#define KRYON_RP2040_PIO_COUNT      4
#define KRYON_RP2040_SRAM_SIZE_KB   264

// ============================================================================
// Platform Lifecycle
// ============================================================================

bool kryon_rp2040_platform_init(void);
void kryon_rp2040_platform_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // KRYON_RP2040_PLATFORM_H