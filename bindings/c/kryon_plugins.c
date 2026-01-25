/**
 * Kryon Plugin Stubs - C Bindings
 *
 * Provides stub implementations of plugin APIs (Storage, DateTime, etc.)
 * for generated C code. These are simple in-memory implementations
 * that can be replaced with full plugin integrations later.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "kryon_plugins.h"

// ============================================================================
// Storage Plugin Stubs
// ============================================================================

#define STORAGE_MAX_KEYS 100
#define STORAGE_MAX_KEY_LEN 256
#define STORAGE_MAX_VALUE_LEN 65536

typedef struct {
    char key[STORAGE_MAX_KEY_LEN];
    char value[STORAGE_MAX_VALUE_LEN];
    bool in_use;
} StorageEntry;

static struct {
    StorageEntry entries[STORAGE_MAX_KEYS];
    char app_name[256];
    bool initialized;
    size_t last_load_count;
} g_storage = { .initialized = false, .last_load_count = 0 };

void Storage_init(const char* app_name) {
    if (g_storage.initialized) return;

    memset(&g_storage, 0, sizeof(g_storage));
    if (app_name) {
        strncpy(g_storage.app_name, app_name, sizeof(g_storage.app_name) - 1);
    }
    g_storage.initialized = true;
}

void* Storage_load(const char* key, void* default_value) {
    (void)key;
    (void)default_value;
    g_storage.last_load_count = 0;
    // Return NULL (empty array) - full implementation would parse stored JSON
    return NULL;
}

size_t Storage_get_last_count(void) {
    return g_storage.last_load_count;
}

bool Storage_save(const char* key, void* data) {
    (void)key;
    (void)data;
    return true;  // Pretend success
}

bool Storage_has(const char* key) {
    (void)key;
    return false;
}

void Storage_remove(const char* key) {
    (void)key;
}

void Storage_clear(void) {
    // No-op
}

void Storage_shutdown(void) {
    g_storage.initialized = false;
}

// ============================================================================
// DateTime Plugin Stubs
// ============================================================================

// KryonDate is already defined in kryon_plugins.h

static KryonDate g_date;

static void fill_kryon_date(KryonDate* d, struct tm* tm) {
    d->year = tm->tm_year + 1900;
    d->month = tm->tm_mon + 1;
    d->day = tm->tm_mday;
    d->hour = tm->tm_hour;
    d->minute = tm->tm_min;
    d->second = tm->tm_sec;
    snprintf(d->iso_string, sizeof(d->iso_string), "%04d-%02d-%02d",
             d->year, d->month, d->day);
}

KryonDate* DateTime_today(void) {
    time_t now = time(NULL);
    struct tm* local = localtime(&now);
    if (local) {
        fill_kryon_date(&g_date, local);
        g_date.hour = 0;
        g_date.minute = 0;
        g_date.second = 0;
    }
    return &g_date;
}

KryonDate* DateTime_now(void) {
    time_t now = time(NULL);
    struct tm* local = localtime(&now);
    if (local) {
        fill_kryon_date(&g_date, local);
    }
    return &g_date;
}

const char* DateTime_toString(KryonDate* date) {
    if (!date) return "";
    return date->iso_string;
}

// Helper for generated code that calls today.toString()
const char* today_toString(void) {
    return DateTime_toString(DateTime_today());
}

int DateTime_weekday(KryonDate* date) {
    if (!date) return 0;
    int y = date->year;
    int m = date->month;
    int d = date->day;
    if (m < 3) { m += 12; y--; }
    int k = y % 100;
    int j = y / 100;
    int h = (d + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    return ((h + 6) % 7);
}

bool DateTime_isFuture(KryonDate* date) {
    if (!date) return false;
    KryonDate* today = DateTime_today();
    if (date->year > today->year) return true;
    if (date->year < today->year) return false;
    if (date->month > today->month) return true;
    if (date->month < today->month) return false;
    return date->day > today->day;
}

static bool datetime_is_leap_year(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

static const int DATETIME_DAYS_IN_MONTH[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

int DateTime_daysInMonth(int year, int month) {
    if (month < 1 || month > 12) return 0;
    if (month == 2 && datetime_is_leap_year(year)) return 29;
    return DATETIME_DAYS_IN_MONTH[month - 1];
}

// ============================================================================
// Math Plugin Stubs (if needed)
// ============================================================================

double Math_random(void) {
    return (double)rand() / (double)RAND_MAX;
}

int Math_floor(double x) {
    return (int)x;
}

int Math_ceil(double x) {
    return (int)(x + 0.9999999);
}

int Math_round(double x) {
    return (int)(x + 0.5);
}

double Math_abs(double x) {
    return x < 0 ? -x : x;
}

double Math_min(double a, double b) {
    return a < b ? a : b;
}

double Math_max(double a, double b) {
    return a > b ? a : b;
}
