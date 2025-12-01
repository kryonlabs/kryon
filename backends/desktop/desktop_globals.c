// ============================================================================
// Desktop Renderer - Global State Definitions
// ============================================================================
// This file defines all global variables shared across desktop renderer modules.
// These are declared as 'extern' in desktop_internal.h and defined here.

#include "desktop_internal.h"
#include <string.h>

// ============================================================================
// GLOBAL RENDERER STATE
// ============================================================================

// Global desktop renderer pointer (for layout measurement during layout calculation)
struct DesktopIRRenderer* g_desktop_renderer = NULL;

// Frame counter for LRU cache management
uint64_t g_frame_counter = 0;

// Debug logging flag (controlled by KRYON_DEBUG_RENDERER environment variable)
bool g_debug_renderer = false;

// ============================================================================
// FONT MANAGEMENT STATE
// ============================================================================

// Font name to path registry (max 32 registered fonts)
RegisteredFont g_font_registry[32];
int g_font_registry_count = 0;

// TTF_Font cache (path+size → font, max 64 cached fonts)
CachedFont g_font_cache[64];
int g_font_cache_count = 0;

// Default font (fallback when component doesn't specify a font)
char g_default_font_name[128] = {0};
char g_default_font_path[512] = {0};

// ============================================================================
// TEXT RENDERING CACHE
// ============================================================================

// Text texture cache with LRU eviction (128 entries, 40-60% speedup)
TextTextureCache g_text_texture_cache[TEXT_TEXTURE_CACHE_SIZE];

// Hash table for O(1) text cache lookup (Phase 1 optimization: 15-25% speedup)
TextCacheHashBucket g_text_cache_hash_table[TEXT_CACHE_HASH_SIZE];

// ============================================================================
// MARKDOWN STATE
// ============================================================================

// Scroll state for markdown components (max 32 markdown components)
MarkdownScrollState markdown_scroll_states[32];
size_t markdown_scroll_state_count = 0;

// ============================================================================
// INPUT STATE
// ============================================================================

// Runtime state for text input components (cursor, scroll, focus, max 64 inputs)
InputRuntimeState input_states[64];
size_t input_state_count = 0;
