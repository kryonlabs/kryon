#ifndef IR_COMPONENT_TABS_H
#define IR_COMPONENT_TABS_H

#include "../ir_core.h"
#include "ir_component_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

// Layout function for TabGroup component
void layout_tab_group_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                   float parent_x, float parent_y);

// TabGroup layout trait (extern for registration)
extern const IRLayoutTrait IR_TAB_GROUP_LAYOUT_TRAIT;

// Layout function for TabBar component
void layout_tab_bar_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                  float parent_x, float parent_y);

// TabBar layout trait (extern for registration)
extern const IRLayoutTrait IR_TAB_BAR_LAYOUT_TRAIT;

// Layout function for Tab component
void layout_tab_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                             float parent_x, float parent_y);

// Tab layout trait (extern for registration)
extern const IRLayoutTrait IR_TAB_LAYOUT_TRAIT;

// Layout function for TabContent component
void layout_tab_content_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                     float parent_x, float parent_y);

// TabContent layout trait (extern for registration)
extern const IRLayoutTrait IR_TAB_CONTENT_LAYOUT_TRAIT;

// Layout function for TabPanel component
void layout_tab_panel_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                                   float parent_x, float parent_y);

// TabPanel layout trait (extern for registration)
extern const IRLayoutTrait IR_TAB_PANEL_LAYOUT_TRAIT;

// Initialize Tab component layout traits
void ir_tab_components_init(void);

#ifdef __cplusplus
}
#endif

#endif // IR_COMPONENT_TABS_H
