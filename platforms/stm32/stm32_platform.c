/*
 * STM32 Platform Integration Kit
 *
 * Provides STM32F4-specific implementations for Kryon framework
 * - HAL integration for peripherals
 * - Flash-based storage
 * - Low-power management
 * - Clock and timing utilities
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// STM32F4 HAL includes
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash.h"

// Include Kryon core header
#include "../../core/include/kryon.h"

// ============================================================================
// STM32 Flash Storage Implementation
// ============================================================================

#define KRYON_STM32_STORAGE_SECTOR  FLASH_SECTOR_3  // Use sector 3 (0x08008000)
#define KRYON_STM32_STORAGE_BASE   0x08008000
#define KRYON_STM32_STORAGE_SIZE   0x20000  // 128KB
#define KRYON_STM32_MAX_ENTRIES    16
#define KRYON_STM32_MAX_KEY_LEN    32
#define KRYON_STM32_MAX_VALUE_LEN  64

typedef struct {
    uint8_t used;
    char key[KRYON_STM32_MAX_KEY_LEN];
    uint8_t value_len;
    uint8_t value[KRYON_STM32_MAX_VALUE_LEN];
    uint32_t checksum;
} kryon_stm32_entry_t;

static bool g_flash_initialized = false;

uint32_t kryon_stm32_calculate_checksum(const uint8_t* data, size_t size) {
    uint32_t checksum = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        checksum ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (checksum & 1) {
                checksum = (checksum >> 1) ^ 0xEDB88320;
            } else {
                checksum >>= 1;
            }
        }
    }
    return ~checksum;
}

bool kryon_stm32_init_storage(void) {
    if (g_flash_initialized) {
        return true;
    }

    // Unlock flash
    HAL_FLASH_Unlock();

    g_flash_initialized = true;
    return true;
}

bool kryon_stm32_storage_set(const char* key, const void* data, size_t size) {
    if (!g_flash_initialized || !key || !data || size == 0 || size > KRYON_STM32_MAX_VALUE_LEN) {
        return false;
    }

    if (strlen(key) > KRYON_STM32_MAX_KEY_LEN - 1) {
        return false;
    }

    // Read current entries to find empty slot
    kryon_stm32_entry_t entries[KRYON_STM32_MAX_ENTRIES];
    uint32_t addr = KRYON_STM32_STORAGE_BASE;
    memcpy(entries, (void*)addr, sizeof(entries));

    // Find existing entry or empty slot
    int entry_index = -1;
    for (int i = 0; i < KRYON_STM32_MAX_ENTRIES; i++) {
        if (!entries[i].used) {
            entry_index = i;
            break;
        }
        if (strncmp(entries[i].key, key, KRYON_STM32_MAX_KEY_LEN) == 0) {
            entry_index = i;
            break;
        }
    }

    if (entry_index == -1) {
        return false; // No space available
    }

    // Prepare new entry
    kryon_stm32_entry_t new_entry = {0};
    new_entry.used = 1;
    strncpy(new_entry.key, key, KRYON_STM32_MAX_KEY_LEN - 1);
    new_entry.value_len = (uint8_t)size;
    memcpy(new_entry.value, data, size);
    new_entry.checksum = kryon_stm32_calculate_checksum((uint8_t*)&new_entry, sizeof(new_entry) - sizeof(uint32_t));

    // Erase sector (this is slow, in real implementation would use wear leveling)
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_SECTORS,
        .Sector = KRYON_STM32_STORAGE_SECTOR,
        .NbSectors = 1,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3
    };

    uint32_t sectorError;
    if (HAL_FLASHEx_Erase(&erase, &sectorError) != HAL_OK) {
        return false;
    }

    // Update entry in memory
    entries[entry_index] = new_entry;

    // Write all entries back to flash
    addr = KRYON_STM32_STORAGE_BASE;
    for (int i = 0; i < KRYON_STM32_MAX_ENTRIES; i++) {
        for (size_t j = 0; j < sizeof(kryon_stm32_entry_t); j += 4) {
            uint32_t word = *((uint32_t*)((uint8_t*)&entries[i] + j));
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, word) != HAL_OK) {
                HAL_FLASH_Lock();
                return false;
            }
            addr += 4;
        }
    }

    HAL_FLASH_Lock();
    return true;
}

bool kryon_stm32_storage_get(const char* key, void* buffer, size_t* size) {
    if (!g_flash_initialized || !key || !buffer || !size) {
        return false;
    }

    // Read entries from flash
    kryon_stm32_entry_t entries[KRYON_STM32_MAX_ENTRIES];
    memcpy(entries, (void*)KRYON_STM32_STORAGE_BASE, sizeof(entries));

    // Find entry
    for (int i = 0; i < KRYON_STM32_MAX_ENTRIES; i++) {
        if (!entries[i].used) {
            continue;
        }

        if (strncmp(entries[i].key, key, KRYON_STM32_MAX_KEY_LEN) == 0) {
            // Verify checksum
            uint32_t calc_checksum = kryon_stm32_calculate_checksum((uint8_t*)&entries[i], sizeof(entries[i]) - sizeof(uint32_t));
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

bool kryon_stm32_storage_has(const char* key) {
    if (!g_flash_initialized || !key) {
        return false;
    }

    // Read entries from flash
    kryon_stm32_entry_t entries[KRYON_STM32_MAX_ENTRIES];
    memcpy(entries, (void*)KRYON_STM32_STORAGE_BASE, sizeof(entries));

    // Search for entry
    for (int i = 0; i < KRYON_STM32_MAX_ENTRIES; i++) {
        if (!entries[i].used) {
            continue;
        }

        if (strncmp(entries[i].key, key, KRYON_STM32_MAX_KEY_LEN) == 0) {
            // Verify checksum
            uint32_t calc_checksum = kryon_stm32_calculate_checksum((uint8_t*)&entries[i], sizeof(entries[i]) - sizeof(uint32_t));
            return calc_checksum == entries[i].checksum;
        }
    }

    return false;
}

// ============================================================================
// STM32 Timing and Clock Utilities
// ============================================================================

uint32_t kryon_stm32_get_timestamp(void) {
    // Return milliseconds since startup using HAL tick
    return HAL_GetTick();
}

void kryon_stm32_delay_ms(uint32_t ms) {
    HAL_Delay(ms);
}

uint32_t kryon_stm32_get_system_clock_hz(void) {
    return SystemCoreClock;
}

bool kryon_stm32_init_clock(void) {
    // This would be called during system initialization
    // For now, assume HAL has already configured the clock
    return SystemCoreClock > 0;
}

// ============================================================================
// STM32 Display Integration (Framebuffer)
// ============================================================================

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t* framebuffer;
    bool initialized;
} kryon_stm32_display_t;

static kryon_stm32_display_t g_display = {0};

bool kryon_stm32_init_display(uint16_t width, uint16_t height, uint8_t* framebuffer) {
    if (g_display.initialized) {
        return true;
    }

    if (!framebuffer) {
        return false;
    }

    g_display.width = width;
    g_display.height = height;
    g_display.framebuffer = framebuffer;
    g_display.initialized = true;

    // Initialize display hardware (SPI LCD, etc.)
    // This would be implementation-specific based on the connected display

    return true;
}

void kryon_stm32_update_display(void) {
    if (!g_display.initialized) {
        return;
    }

    // Transfer framebuffer to display hardware
    // This implementation depends on the specific display interface
    // (SPI for ILI9341, parallel for TFT, etc.)
}

uint16_t kryon_stm32_get_display_width(void) {
    return g_display.width;
}

uint16_t kryon_stm32_get_display_height(void) {
    return g_display.height;
}

uint8_t* kryon_stm32_get_framebuffer(void) {
    return g_display.framebuffer;
}

// ============================================================================
// STM32 GPIO and Input Handling
// ============================================================================

typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
    bool last_state;
    uint32_t last_change_time;
} kryon_stm32_gpio_button_t;

#define KRYON_STM32_MAX_BUTTONS 4
static kryon_stm32_gpio_button_t g_buttons[KRYON_STM32_MAX_BUTTONS];
static uint8_t g_button_count = 0;

bool kryon_stm32_add_button(GPIO_TypeDef* port, uint16_t pin) {
    if (g_button_count >= KRYON_STM32_MAX_BUTTONS) {
        return false;
    }

    g_buttons[g_button_count].port = port;
    g_buttons[g_button_count].pin = pin;
    g_buttons[g_button_count].last_state = false;
    g_buttons[g_button_count].last_change_time = 0;
    g_button_count++;

    return true;
}

bool kryon_stm32_read_button(uint8_t button_index, bool* state) {
    if (button_index >= g_button_count || !state) {
        return false;
    }

    bool current_state = HAL_GPIO_ReadPin(g_buttons[button_index].port, g_buttons[button_index].pin) == GPIO_PIN_SET;
    uint32_t current_time = HAL_GetTick();

    // Debounce - only report state change if stable for 20ms
    if (current_state != g_buttons[button_index].last_state) {
        if (current_time - g_buttons[button_index].last_change_time > 20) {
            g_buttons[button_index].last_state = current_state;
            g_buttons[button_index].last_change_time = current_time;
            *state = current_state;
            return true;
        }
    }

    *state = g_buttons[button_index].last_state;
    return false;
}

// ============================================================================
// STM32 Power Management
// ============================================================================

typedef enum {
    KRYON_STM32_POWER_ACTIVE,
    KRYON_STM32_POWER_SLEEP,
    KRYON_STM32_POWER_STOP,
    KRYON_STM32_POWER_STANDBY
} kryon_stm32_power_mode_t;

bool kryon_stm32_set_power_mode(kryon_stm32_power_mode_t mode) {
    switch (mode) {
        case KRYON_STM32_POWER_ACTIVE:
            // Normal operation
            HAL_ResumeTick();
            break;

        case KRYON_STM32_POWER_SLEEP:
            // Sleep mode (CPU stopped, peripherals running)
            HAL_SuspendTick();
            HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
            HAL_ResumeTick();
            break;

        case KRYON_STM32_POWER_STOP:
            // Stop mode (most clocks stopped, can wake from interrupt)
            HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);
            SystemCoreClockUpdate(); // Reconfigure after wakeup
            break;

        case KRYON_STM32_POWER_STANDBY:
            // Standby mode (lowest power, RTC and wakeup pins only)
            HAL_PWR_EnterSTANDBYMode();
            break;

        default:
            return false;
    }

    return true;
}

// ============================================================================
// STM32 Platform Information
// ============================================================================

void kryon_stm32_print_info(void) {
    printf("=== STM32 Platform Information ===\n");
    printf("Core Clock: %lu Hz\n", SystemCoreClock);
    printf("Flash Size: %lu KB\n", *(__IO uint16_t*)0x1FFF7A22);
    printf("SRAM Size: %lu KB\n", *(__IO uint16_t*)0x1FFF7A20);
    printf("Device ID: %08lX\n", DBGMCU->IDCODE);
    printf("Storage: %d bytes @ 0x%08lX\n", KRYON_STM32_STORAGE_SIZE, KRYON_STM32_STORAGE_BASE);
    printf("================================\n");
}

// ============================================================================
// STM32 Platform Initialization
// ============================================================================

bool kryon_stm32_platform_init(void) {
    // This should be called after HAL_Init()

    // Initialize storage
    if (!kryon_stm32_init_storage()) {
        return false;
    }

    // Initialize clock system
    if (!kryon_stm32_init_clock()) {
        return false;
    }

    printf("STM32 platform initialized\n");
    return true;
}

void kryon_stm32_platform_shutdown(void) {
    // Cleanup storage
    HAL_FLASH_Lock();

    // Enter low power mode
    kryon_stm32_set_power_mode(KRYON_STM32_POWER_STANDBY);
}