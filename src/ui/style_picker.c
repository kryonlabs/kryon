#include "ui_style_sheet.h"
#include "ui_draw.h"
#include "ui_tree.h"

bool
StylePicker(StylePickerProps props)
{
    const char *labels[STYLE_PACK_MAX];
    int selected = -1;
    int count = GetStylePackCount();
    StylePickerState state;

    if(count <= 0 && EnsureBuiltInStylePacks())
        count = GetStylePackCount();
    state = StylePickerStateFor(count, selected, STYLE_PACK_MAX);
    count = state.option_count;
    if(!state.has_options)
        return false;

    for(int i = 0; i < count; i++) {
        const StylePack *pack = GetStylePackAt(i);
        if(pack == NULL)
            return false;
        labels[i] = pack->label != NULL && pack->label[0] != '\0'
            ? pack->label : pack->id;
        if(pack == GetActiveStylePack())
            selected = i;
    }

    state = StylePickerStateFor(count, selected, STYLE_PACK_MAX);
    selected = state.selected_index;

    bool changed = Dropdown((DropdownProps){
        .bounds = props.bounds,
        .id = props.id,
        .options = labels,
        .option_count = count,
        .selected_index = &selected,
        .class_name = props.class_name,
        .disabled = props.disabled,
    });
    if(changed && selected >= 0 && selected < count) {
        const StylePack *pack = GetStylePackAt(selected);
        return pack != NULL && SetActiveStylePack(pack->id);
    }
    return false;
}
