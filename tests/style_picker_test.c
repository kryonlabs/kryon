#include "ui_style_sheet.h"
#include <assert.h>
#include <string.h>

static int dropdown_calls;
static int dropdown_changed;
static int dropdown_next_index;
static DropdownProps captured_dropdown;

void
PushInspectSource(const char *path, int line)
{
    (void)path;
    (void)line;
}

void
PopInspectSource(void)
{
}

int
Dropdown(DropdownProps dropdown)
{
    dropdown_calls++;
    captured_dropdown = dropdown;
    if(dropdown.selected_index != NULL && dropdown_next_index >= 0)
        *dropdown.selected_index = dropdown_next_index;
    return dropdown_changed;
}

int
main(void)
{
    StyleSheet sheet = {0};
    StylePickerState picker_state;

    ClearStylePacks();
    dropdown_calls = 0;
    dropdown_changed = 0;
    dropdown_next_index = 0;
    assert(StylePickerOptionCountFor(-1, 32) == 0);
    assert(StylePickerOptionCountFor(40, 32) == 32);
    assert(StylePickerOptionCountFor(4, 0) == 4);
    assert(StylePickerSelectedIndexFor(-1, 3) == 0);
    assert(StylePickerSelectedIndexFor(9, 3) == 2);
    assert(StylePickerSelectedIndexFor(0, 0) == -1);
    picker_state = StylePickerStateFor(12, 99, 8);
    assert(picker_state.has_options);
    assert(picker_state.option_count == 8);
    assert(picker_state.selected_index == 7);
    picker_state = StylePickerStateFor(-2, 0, 8);
    assert(!picker_state.has_options);
    assert(picker_state.option_count == 0);
    assert(picker_state.selected_index == -1);
    assert(!StylePicker((StylePickerProps){.id = 41}));
    assert(dropdown_calls == 1);
    assert(captured_dropdown.option_count == 3);
    assert(captured_dropdown.options != NULL);
    assert(strcmp(captured_dropdown.options[0], "Material") == 0);
    assert(GetActiveStylePack() != NULL);
    assert(strcmp(GetActiveStylePack()->id, "material") == 0);

    ClearStylePacks();

    assert(RegisterStylePack((StylePack){
        .id = "first",
        .label = "First",
        .description = "Default look",
        .sheet = &sheet,
    }));
    assert(RegisterStylePack((StylePack){
        .id = "glow",
        .label = "Glow",
        .description = "Glow look",
        .sheet = &sheet,
    }));

    dropdown_calls = 0;
    dropdown_changed = 0;
    dropdown_next_index = 0;
    assert(!StylePicker((StylePickerProps){
        .bounds = {1, 2, 120, 28},
        .id = 42,
    }));
    assert(dropdown_calls == 1);
    assert(captured_dropdown.id == 42);
    assert(captured_dropdown.option_count == 2);
    assert(captured_dropdown.options != NULL);
    assert(strcmp(captured_dropdown.options[0], "First") == 0);
    assert(strcmp(captured_dropdown.options[1], "Glow") == 0);
    assert(GetActiveStylePack() != NULL);
    assert(strcmp(GetActiveStylePack()->id, "first") == 0);

    dropdown_changed = 1;
    dropdown_next_index = 1;
    assert(StylePicker((StylePickerProps){
        .bounds = {1, 2, 120, 28},
        .id = 42,
        .disabled = 1,
    }));
    assert(captured_dropdown.disabled == 1);
    assert(GetActiveStylePack() != NULL);
    assert(strcmp(GetActiveStylePack()->id, "glow") == 0);

    ClearStylePacks();
    return 0;
}
