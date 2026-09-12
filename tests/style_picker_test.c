#include "ui_style_sheet.h"
#include <assert.h>
#include <string.h>

static int dropdown_calls;
static int dropdown_changed;
static int dropdown_next_index;
static DropdownProps captured_dropdown;

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

    ClearStylePacks();
    dropdown_calls = 0;
    dropdown_changed = 0;
    dropdown_next_index = 0;
    assert(!StylePicker((StylePickerProps){.id = 41}));
    assert(dropdown_calls == 1);
    assert(captured_dropdown.option_count >= 5);
    assert(captured_dropdown.options != NULL);
    assert(strcmp(captured_dropdown.options[0], "Material") == 0);
    assert(GetActiveStylePack() != NULL);
    assert(strcmp(GetActiveStylePack()->id, "kryon.material") == 0);

    ClearStylePacks();

    assert(RegisterStylePack((StylePack){
        .id = "vanilla",
        .label = "Vanilla",
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
    assert(strcmp(captured_dropdown.options[0], "Vanilla") == 0);
    assert(strcmp(captured_dropdown.options[1], "Glow") == 0);
    assert(GetActiveStylePack() != NULL);
    assert(strcmp(GetActiveStylePack()->id, "vanilla") == 0);

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
