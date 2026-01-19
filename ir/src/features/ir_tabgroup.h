/*
 * Kryon IR - TabGroup State Management
 *
 * Handles tab group state including:
 * - Tab selection and switching
 * - Panel visibility management
 * - Tab reordering (drag and drop)
 * - Visual state (active/inactive colors)
 * - User callbacks for tab clicks
 */

#ifndef IR_TABGROUP_H
#define IR_TABGROUP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct IRComponent;
typedef struct IRComponent IRComponent;

// ============================================================================
// Types
// ============================================================================

/**
 * Visual state for a single tab (colors for active/inactive states)
 */
typedef struct TabVisualState {
    uint32_t background_color;        // Inactive background
    uint32_t active_background_color; // Active background
    uint32_t text_color;              // Inactive text color
    uint32_t active_text_color;       // Active text color
} TabVisualState;

/**
 * User callback for tab click events
 * Called before tab selection, allowing user code to react first
 */
typedef void (*TabClickCallback)(uint32_t tab_index, void* user_data);

/**
 * Maximum number of tabs per group (fixed-size arrays for performance)
 */
#define IR_MAX_TABS_PER_GROUP 32

/**
 * TabGroup state - manages tabs, panels, and visual state
 *
 * This is stored in component->custom_data for TabGroup components.
 * The fixed-size arrays avoid dynamic allocation for better performance.
 */
typedef struct TabGroupState {
    struct IRComponent* group;                          // TabGroup component
    struct IRComponent* tab_bar;                        // TabBar component
    struct IRComponent* tab_content;                    // TabContent component

    // Fixed-size arrays (no dynamic allocation)
    struct IRComponent* tabs[IR_MAX_TABS_PER_GROUP];         // Tab components
    struct IRComponent* panels[IR_MAX_TABS_PER_GROUP];       // Panel components
    TabVisualState tab_visuals[IR_MAX_TABS_PER_GROUP];       // Visual state for each tab
    TabClickCallback user_callbacks[IR_MAX_TABS_PER_GROUP];  // User callbacks
    void* user_callback_data[IR_MAX_TABS_PER_GROUP];         // User callback data

    uint32_t tab_count;       // Number of registered tabs
    uint32_t panel_count;     // Number of registered panels
    int selected_index;       // Currently selected tab index

    bool reorderable;         // Whether tabs can be reordered via drag
    bool dragging;            // Currently dragging a tab
    int drag_index;           // Index of tab being dragged
    float drag_x;             // X position of drag
} TabGroupState;

// ============================================================================
// Lifecycle
// ============================================================================

/**
 * Create a new TabGroup state
 *
 * @param group TabGroup component
 * @param tab_bar TabBar component
 * @param tab_content TabContent component
 * @param selected_index Initially selected tab index
 * @param reorderable Whether tabs can be reordered
 * @return New TabGroup state, or NULL on failure
 */
TabGroupState* ir_tabgroup_create_state(IRComponent* group,
                                        IRComponent* tab_bar,
                                        IRComponent* tab_content,
                                        int selected_index,
                                        bool reorderable);

/**
 * Destroy TabGroup state
 *
 * @param state TabGroup state to destroy
 */
void ir_tabgroup_destroy_state(TabGroupState* state);

/**
 * Finalize TabGroup state - extract colors from Tab components and apply initial selection
 * Call this after all tabs/panels are registered
 *
 * @param state TabGroup state to finalize
 */
void ir_tabgroup_finalize(TabGroupState* state);

// ============================================================================
// Registration
// ============================================================================

/**
 * Register TabBar component
 *
 * @param state TabGroup state
 * @param tab_bar TabBar component
 */
void ir_tabgroup_register_bar(TabGroupState* state, IRComponent* tab_bar);

/**
 * Register TabContent component
 *
 * @param state TabGroup state
 * @param tab_content TabContent component
 */
void ir_tabgroup_register_content(TabGroupState* state, IRComponent* tab_content);

/**
 * Register a tab component
 *
 * @param state TabGroup state
 * @param tab Tab component to register
 */
void ir_tabgroup_register_tab(TabGroupState* state, IRComponent* tab);

/**
 * Register a panel component
 *
 * @param state TabGroup state
 * @param panel Panel component to register
 */
void ir_tabgroup_register_panel(TabGroupState* state, IRComponent* panel);

// ============================================================================
// Selection and Interaction
// ============================================================================

/**
 * Select a tab by index
 * This switches the visible panel and updates visual states
 *
 * @param state TabGroup state
 * @param index Tab index to select
 */
void ir_tabgroup_select(TabGroupState* state, int index);

/**
 * Handle tab click - calls user callback then selects tab
 *
 * @param state TabGroup state
 * @param tab_index Index of clicked tab
 */
void ir_tabgroup_handle_tab_click(TabGroupState* state, uint32_t tab_index);

/**
 * Set user callback for a specific tab
 *
 * @param state TabGroup state
 * @param index Tab index
 * @param callback Callback function
 * @param user_data User data to pass to callback
 */
void ir_tabgroup_set_tab_callback(TabGroupState* state, uint32_t index,
                                   TabClickCallback callback, void* user_data);

// ============================================================================
// Reordering
// ============================================================================

/**
 * Reorder tabs from one index to another
 *
 * @param state TabGroup state
 * @param from_index Source index
 * @param to_index Destination index
 */
void ir_tabgroup_reorder(TabGroupState* state, int from_index, int to_index);

/**
 * Handle drag events for tab reordering
 *
 * @param state TabGroup state
 * @param x Mouse X position
 * @param y Mouse Y position
 * @param is_down Mouse button pressed
 * @param is_up Mouse button released
 */
void ir_tabgroup_handle_drag(TabGroupState* state, float x, float y, bool is_down, bool is_up);

/**
 * Set whether tabs are reorderable
 *
 * @param state TabGroup state
 * @param reorderable True to enable reordering
 */
void ir_tabgroup_set_reorderable(TabGroupState* state, bool reorderable);

// ============================================================================
// Visual State
// ============================================================================

/**
 * Set visual state for a specific tab
 *
 * @param state TabGroup state
 * @param index Tab index
 * @param visual Visual state to set
 */
void ir_tabgroup_set_tab_visual(TabGroupState* state, int index, TabVisualState visual);

/**
 * Apply visual states to all tabs (active/inactive colors)
 *
 * @param state TabGroup state
 */
void ir_tabgroup_apply_visuals(TabGroupState* state);

// ============================================================================
// Query Functions
// ============================================================================

/**
 * Get number of tabs
 *
 * @param state TabGroup state
 * @return Number of registered tabs
 */
uint32_t ir_tabgroup_get_tab_count(TabGroupState* state);

/**
 * Get number of panels
 *
 * @param state TabGroup state
 * @return Number of registered panels
 */
uint32_t ir_tabgroup_get_panel_count(TabGroupState* state);

/**
 * Get currently selected tab index
 *
 * @param state TabGroup state
 * @return Selected index, or -1 if none selected
 */
int ir_tabgroup_get_selected(TabGroupState* state);

/**
 * Get tab component at index
 *
 * @param state TabGroup state
 * @param index Tab index
 * @return Tab component, or NULL if index invalid
 */
IRComponent* ir_tabgroup_get_tab(TabGroupState* state, uint32_t index);

/**
 * Get panel component at index
 *
 * @param state TabGroup state
 * @param index Panel index
 * @return Panel component, or NULL if index invalid
 */
IRComponent* ir_tabgroup_get_panel(TabGroupState* state, uint32_t index);

#ifdef __cplusplus
}
#endif

#endif // IR_TABGROUP_H
