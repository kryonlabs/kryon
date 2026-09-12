#include "ui_style_sheet.h"
#include "ui_draw.h"
#include "ui_tree.h"

bool
StylePicker(StylePickerProps props)
{
    const char *labels[STYLE_PACK_MAX];
    int selected = -1;
    int count = GetStylePackCount();

    if(count <= 0)
        return false;
    if(count > STYLE_PACK_MAX)
        count = STYLE_PACK_MAX;

    for(int i = 0; i < count; i++) {
        const StylePack *pack = GetStylePackAt(i);
        if(pack == NULL)
            return false;
        labels[i] = pack->label != NULL && pack->label[0] != '\0'
            ? pack->label : pack->id;
        if(pack == GetActiveStylePack())
            selected = i;
    }

    if(selected < 0)
        selected = 0;

    bool changed = Dropdown((DropdownProps){
        .bounds = props.bounds,
        .id = props.id,
        .options = labels,
        .option_count = count,
        .selected_index = &selected,
        .disabled = props.disabled,
    });
    if(changed && selected >= 0 && selected < count) {
        const StylePack *pack = GetStylePackAt(selected);
        return pack != NULL && SetActiveStylePack(pack->id);
    }
    return false;
}
