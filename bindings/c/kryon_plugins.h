/**
 * Kryon Plugin Stubs - C Header
 *
 * Provides declarations for plugin APIs (Storage, DateTime, etc.)
 * that are used by generated C code.
 */

#ifndef KRYON_PLUGINS_H
#define KRYON_PLUGINS_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// DateTime Types
// ============================================================================

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    char iso_string[32];
} KryonDate;

// ============================================================================
// Storage API
// ============================================================================

void Storage_init(const char* app_name);
void* Storage_load(const char* key, void* default_value);
size_t Storage_get_last_count(void);
bool Storage_save(const char* key, void* data);
bool Storage_has(const char* key);
void Storage_remove(const char* key);
void Storage_clear(void);
void Storage_shutdown(void);

// ============================================================================
// DateTime API
// ============================================================================

KryonDate* DateTime_today(void);
KryonDate* DateTime_now(void);
const char* DateTime_toString(KryonDate* date);
const char* today_toString(void);  // Helper for today.toString()
int DateTime_weekday(KryonDate* date);
bool DateTime_isFuture(KryonDate* date);
int DateTime_daysInMonth(int year, int month);

// ============================================================================
// Math API
// ============================================================================

double Math_random(void);
int Math_floor(double x);
int Math_ceil(double x);
int Math_round(double x);
double Math_abs(double x);
double Math_min(double a, double b);
double Math_max(double a, double b);

#ifdef __cplusplus
}
#endif

#endif // KRYON_PLUGINS_H
