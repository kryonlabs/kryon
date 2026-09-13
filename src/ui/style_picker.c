#include "ui_style_sheet.h"
#include "ui_draw.h"
#include "ui_tree.h"

bool
StylePicker(StylePickerProps props)
{
    const char *labels[STYLE_PACK_MAX];
    int selected = -1;
    int count = GetStylePackCount();

    if(count <= 0 && EnsureBuiltInStylePacks())
        count = GetStylePackCount();
    count = StylePickerOptionCountFor(count, STYLE_PACK_MAX);
    if(count <= 0)
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

    selected = StylePickerSelectedIndexFor(selected, count);

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
