/*
 * STM32 Platform Integration Kit - Header
 *
 * Public interface for STM32F4-specific Kryon functionality
 */

#ifndef KRYON_STM32_PLATFORM_H
#define KRYON_STM32_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// STM32 HAL includes (conditional)
#ifdef KRYON_STM32_HAL_AVAILABLE
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_gpio.h"
#else
// Fallback declarations for compilation without HAL
typedef void* GPIO_TypeDef;
#define GPIO_PIN_SET 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Storage Implementation
// ============================================================================

bool kryon_stm32_init_storage(void);

bool kryon_stm32_storage_set(const char* key, const void* data, size_t size);
bool kryon_stm32_storage_get(const char* key, void* buffer, size_t* size);
bool kryon_stm32_storage_has(const char* key);

// Storage limits
#define KRYON_STM32_MAX_KEY_LEN    32
#define KRYON_STM32_MAX_VALUE_LEN  64
#define KRYON_STM32_MAX_ENTRIES    16

// ============================================================================
// Timing and Clock Utilities
// ============================================================================

uint32_t kryon_stm32_get_timestamp(void);
void kryon_stm32_delay_ms(uint32_t ms);
uint32_t kryon_stm32_get_system_clock_hz(void);
bool kryon_stm32_init_clock(void);

// ============================================================================
// Display Integration
// ============================================================================

bool kryon_stm32_init_display(uint16_t width, uint16_t height, uint8_t* framebuffer);
void kryon_stm32_update_display(void);

uint16_t kryon_stm32_get_display_width(void);
uint16_t kryon_stm32_get_display_height(void);
uint8_t* kryon_stm32_get_framebuffer(void);

// ============================================================================
// GPIO and Input Handling
// ============================================================================

bool kryon_stm32_add_button(GPIO_TypeDef* port, uint16_t pin);
bool kryon_stm32_read_button(uint8_t button_index, bool* state);

#define KRYON_STM32_MAX_BUTTONS 4

// ============================================================================
// Power Management
// ============================================================================

typedef enum {
    KRYON_STM32_POWER_ACTIVE,
    KRYON_STM32_POWER_SLEEP,
    KRYON_STM32_POWER_STOP,
    KRYON_STM32_POWER_STANDBY
} kryon_stm32_power_mode_t;

bool kryon_stm32_set_power_mode(kryon_stm32_power_mode_t mode);

// ============================================================================
// Platform Information
// ============================================================================

void kryon_stm32_print_info(void);

// ============================================================================
// Platform Lifecycle
// ============================================================================

bool kryon_stm32_platform_init(void);
void kryon_stm32_platform_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // KRYON_STM32_PLATFORM_H