#include "ui_node_registry.h"

#include <stdio.h>
#include <string.h>

#define EDITABLE (KRYON_NODE_SELECTABLE | KRYON_NODE_MOVABLE | \
                  KRYON_NODE_RESIZABLE)
#define INSERT_EDITABLE (KRYON_NODE_INSERTABLE | EDITABLE)
#define MOVABLE_TEXT (KRYON_NODE_INSERTABLE | KRYON_NODE_SELECTABLE | \
                      KRYON_NODE_MOVABLE)

static const KryonNodeType kryon_node_types[] = {
    {"Background", "Background", "UI/Display", "Control", "Fill", KRYON_NODE_INSERTABLE | KRYON_NODE_SELECTABLE},
    {"Text", "Text", "UI/Display", "Control", "Label", MOVABLE_TEXT},
    {"Paragraph", "Paragraph", "UI/Display", "Control", "Rich text", KRYON_NODE_SELECTABLE},
    {"Rect", "Rect", "UI/Display", "Control", "Shape", INSERT_EDITABLE},
    {"Line", "Line", "UI/Display", "Control", "Stroke", INSERT_EDITABLE},
    {"Bevel", "Bevel", "UI/Display", "Control", "Relief", EDITABLE},
    {"Icon", "Icon", "UI/Display", "Control", "Icon", EDITABLE},
    {"Image", "Image", "UI/Display", "Control", "Image", INSERT_EDITABLE},

    {"Card", "Card", "UI/Input", "Control", "Surface action", INSERT_EDITABLE},
    {"Button", "Button", "UI/Input", "Control", "Action", INSERT_EDITABLE},
    {"Link", "Link", "UI/Input", "Control", "Link", EDITABLE},
    {"TextField", "Text Field", "UI/Input", "Control", "Input", INSERT_EDITABLE},
    {"Dropdown", "Dropdown", "UI/Input", "Control", "Selection", INSERT_EDITABLE},
    {"Slider", "Slider", "UI/Input", "Control", "Value", INSERT_EDITABLE},
    {"Toggle", "Toggle", "UI/Input", "Control", "On/off", INSERT_EDITABLE},
    {"Checkbox", "Checkbox", "UI/Input", "Control", "Boolean", INSERT_EDITABLE},
    {"Radio", "Radio", "UI/Input", "Control", "Choice", EDITABLE},
    {"Progress", "Progress", "UI/Input", "Control", "Progress", EDITABLE},
    {"Spinbox", "Spinbox", "UI/Input", "Control", "Number", EDITABLE},
    {"ColorPicker", "Color Picker", "UI/Input", "Control", "Color", EDITABLE},

    {"Group", "Group", "UI/Layout", "Control", "Container", INSERT_EDITABLE},
    {"Separator", "Separator", "UI/Layout", "Control", "Divider", EDITABLE},
    {"LabelFrame", "Label Frame", "UI/Layout", "Control", "Frame", EDITABLE},
    {"PanedView", "Paned View", "UI/Layout", "Control", "Split panes", EDITABLE},
    {"Collapsible", "Collapsible", "UI/Layout", "Control", "Section", EDITABLE},

    {"ListBox", "List Box", "UI/Collections", "Control", "List", EDITABLE},
    {"TreeView", "Tree View", "UI/Collections", "Control", "Tree", EDITABLE},
    {"TableView", "Table View", "UI/Collections", "Control", "Table", EDITABLE},
    {"TextArea", "Text Area", "UI/Collections", "Control", "Text area", EDITABLE},
    {"CanvasGrid", "Canvas Grid", "UI/Collections", "Control", "Grid", EDITABLE},

    {"MenuBar", "Menu Bar", "UI/Navigation", "Control", "Menu", EDITABLE},
    {"PopupMenu", "Popup Menu", "UI/Navigation", "Control", "Menu", EDITABLE},
    {"NavigationBar", "Navigation Bar", "UI/Navigation", "Control", "Tabs", EDITABLE},
    {"Toolbar", "Toolbar", "UI/Navigation", "Control", "Tools", EDITABLE},
    {"TabBar", "Tab Bar", "UI/Navigation", "Control", "Tabs", EDITABLE},
    {"TitleBar", "Title Bar", "UI/Navigation", "Control", "Title", EDITABLE},

    {"Focus", "Focus", "UI/Overlays", "Control", "Focus", EDITABLE},
    {"FocusDebugOverlay", "Focus Debug Overlay", "UI/Overlays", "Control", "Debug", KRYON_NODE_SELECTABLE},
    {"TransitionFade", "Transition Fade", "UI/Overlays", "Control", "Transition", EDITABLE},
    {"Modal", "Modal", "UI/Overlays", "Control", "Dialog", EDITABLE},


    {"Scene", "Scene", "Game2D/Core", "Node", "Scene root", 0},
    {"Node2D", "Node2D", "Game2D/Core", "Node", "Transform", 0},
    {"Camera2D", "Camera2D", "Game2D/Core", "Node2D", "Camera", 0},
    {"Sprite2D", "Sprite2D", "Game2D/Rendering", "Node2D", "Image", 0},
    {"AnimatedSprite2D", "AnimatedSprite2D", "Game2D/Rendering", "Node2D", "Animation", 0},
    {"TileMap", "TileMap", "Game2D/Rendering", "Node2D", "Tiles", 0},
    {"CollisionShape2D", "Collision Shape 2D", "Game2D/Physics", "Node2D", "Collider", 0},
    {"Area2D", "Area2D", "Game2D/Physics", "Node2D", "Trigger", 0},
    {"Body2D", "Body2D", "Game2D/Physics", "Node2D", "Body", 0},
    {"Timer", "Timer", "Game2D/Runtime", "Node", "Timer", 0},
    {"AudioSource", "Audio Source", "Game2D/Audio", "Node2D", "Sound", 0},
    {"Light2D", "Light2D", "Game2D/Rendering", "Node2D", "Point light", 0}
};

static const KryonNodeType *
kryon_node_type_checked(int index)
{
    if(index < 0 || index >= KryonNodeTypeCount())
        return NULL;
    return &kryon_node_types[index];
}

static int
kryon_node_type_has_snippet(const char *name)
{
    static const char *snippet_names[] = {
        "Background",
        "Text",
        "Rect",
        "Line",
        "Image",
        "Card",
        "Button",
        "TextField",
        "Toggle",
        "Slider",
        "Checkbox",
        "Dropdown",
        "Group"
    };

    if(name == NULL)
        return 0;
    for(size_t i = 0; i < sizeof(snippet_names) / sizeof(snippet_names[0]); i++) {
        if(strcmp(name, snippet_names[i]) == 0)
            return 1;
    }
    return 0;
}

int
KryonNodeTypeCount(void)
{
    return (int)(sizeof(kryon_node_types) / sizeof(kryon_node_types[0]));
}

const KryonNodeType *
KryonNodeTypeAt(int index)
{
    return kryon_node_type_checked(index);
}

const char *
KryonNodeTypeName(int index)
{
    const KryonNodeType *type = kryon_node_type_checked(index);
    return type != NULL ? type->name : "";
}

const char *
KryonNodeTypeLabel(int index)
{
    const KryonNodeType *type = kryon_node_type_checked(index);
    return type != NULL ? type->label : "";
}

const char *
KryonNodeTypeGroup(int index)
{
    const KryonNodeType *type = kryon_node_type_checked(index);
    return type != NULL ? type->group : "";
}

const char *
KryonNodeTypeBase(int index)
{
    const KryonNodeType *type = kryon_node_type_checked(index);
    return type != NULL ? type->base : "";
}

const char *
KryonNodeTypeDetail(int index)
{
    const KryonNodeType *type = kryon_node_type_checked(index);
    return type != NULL ? type->detail : "";
}

unsigned
KryonNodeTypeFlagsAt(int index)
{
    const KryonNodeType *type = kryon_node_type_checked(index);
    return type != NULL ? type->flags : 0;
}

int
KryonNodeTypeInsertable(int index)
{
    const KryonNodeType *type = kryon_node_type_checked(index);

    return type != NULL &&
           (type->flags & KRYON_NODE_INSERTABLE) != 0 &&
           kryon_node_type_has_snippet(type->name);
}

int
KryonNodeTypeSnippet(int index, int x, int y, char *dst, int cap)
{
    const KryonNodeType *type = kryon_node_type_checked(index);
    int id;

    if(dst == NULL || cap <= 0)
        return 0;
    dst[0] = '\0';
    if(type == NULL || !KryonNodeTypeInsertable(index))
        return 0;
    id = (x * 31 + y * 17 + index * 101) & 0x7fffffff;
    if(strcmp(type->name, "Background") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    Background(GetThemeBackground())\n");
    } else if(strcmp(type->name, "Text") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    Text((TextProps){\n"
                 "        .bounds = {Scale(%d), Scale(%d), 0, 0},\n"
                 "        .text = \"Text\",\n"
                 "        .font = Text16,\n"
                 "        .color = GetThemeText(),\n"
                 "        .wrap = TextWrapNone,\n"
                 "    })\n",
                 x, y);
    } else if(strcmp(type->name, "Rect") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    Rect(Scale(%d), Scale(%d), Scale(160), Scale(90), GetThemeButton(), GetThemeButtonHover())\n",
                 x, y);
    } else if(strcmp(type->name, "Line") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    Line(Scale(%d), Scale(%d), Scale(%d), Scale(%d), GetThemeLink())\n",
                 x, y, x + 160, y + 40);
    } else if(strcmp(type->name, "Image") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    Image((ImageProps){\n"
                 "        .asset_path = \"assets/image.png\",\n"
                 "        .bounds = {Scale(%d), Scale(%d), Scale(180), Scale(110)},\n"
                 "        .tint = WHITE,\n"
                 "        .fit = IMAGE_FIT_CONTAIN,\n"
                 "    })\n",
                 x, y);
    } else if(strcmp(type->name, "Card") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    if Card((CardProps){\n"
                 "        .bounds = {Scale(%d), Scale(%d), Scale(180), Scale(96)},\n"
                 "        .clickable = true,\n"
                 "        .id = %d,\n"
                 "    }) {\n"
                 "    }\n",
                 x, y, 5200 + (id % 1000));
    } else if(strcmp(type->name, "Button") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    if Button((ButtonProps){\n"
                 "        .bounds = {Scale(%d), Scale(%d), Scale(140), Scale(36)},\n"
                 "        .label = \"Button\",\n"
                 "        \n"
                 "        .font = Text16,\n"
                 "        .id = %d,\n"
                 "    }) {\n"
                 "    }\n",
                 x, y, 4200 + (id % 1000));
    } else if(strcmp(type->name, "TextField") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    field_%d: [128] char\n"
                 "    field_cursor_%d: int = 0\n"
                 "    field_focused_%d: int = 0\n"
                 "    TextField((TextFieldProps){\n"
                 "        .bounds = {Scale(%d), Scale(%d), Scale(180), Scale(34)},\n"
                 "        .text = field_%d,\n"
                 "        .text_size = sizeof(field_%d),\n"
                 "        .cursor_position = &field_cursor_%d,\n"
                 "        .focused = &field_focused_%d,\n"
                 "        .max_codepoints = 128,\n"
                 "        .font = Text16,\n"
                 "        .focus_id = %d,\n"
                 "        .style = (TextInputStyle){\n"
                 "            GetThemeSurface(), GetThemeButton(), GetThemeLink(),\n"
                 "            GetThemeText(), GetThemeLink(), 0, Scale(8), Scale(6),\n"
                 "        },\n"
                 "    })\n",
                 id, id, id, x, y, id, id, id, id, 5200 + (id % 1000));
    } else if(strcmp(type->name, "Toggle") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    toggle_%d: int = 0\n"
                 "    Toggle((ToggleProps){.bounds = {Scale(%d), Scale(%d), Scale(120), Scale(34)}, .id = %d, .value = &toggle_%d, .off_label = \"Off\", .on_label = \"On\"})\n",
                 id, x, y, 6200 + (id % 1000), id);
    } else if(strcmp(type->name, "Slider") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    slider_%d: [1] int = {50}\n"
                 "    Slider((SliderProps){.bounds = {Scale(%d), Scale(%d), Scale(180), Scale(56)}, .id = %d, .label = \"Value\", .kind = 1, .int_values = slider_%d, .value_count = 1, .min = 0.0, .max = 100.0})\n",
                 id, x, y, 7200 + (id % 1000), id);
    } else if(strcmp(type->name, "Checkbox") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    check_%d: int = 0\n"
                 "    Checkbox((CheckboxProps){.bounds = {Scale(%d), Scale(%d), Scale(140), Scale(34)}, .id = %d, .label = \"Checkbox\", .value = &check_%d})\n",
                 id, x, y, 8200 + (id % 1000), id);
    } else if(strcmp(type->name, "Dropdown") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    options_%d: [3] const char* = {\"One\", \"Two\", \"Three\"}\n"
                 "    selected_%d: int = 0\n"
                 "    Dropdown((DropdownProps){.bounds = {Scale(%d), Scale(%d), Scale(180), Scale(34)}, .id = %d, .options = options_%d, .option_count = 3, .selected_index = &selected_%d})\n",
                 id, id, x, y, 9200 + (id % 1000), id, id);
    } else if(strcmp(type->name, "Group") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    Stack((ColumnProps){.bounds = {Scale(%d), Scale(%d), Scale(180), Scale(110)}, .key = Key(\"group-%d\")})\n"
                 "    Rect(Scale(%d), Scale(%d), Scale(180), Scale(110), Fade(GetThemeButton(), 0.45), GetThemeButtonHover())\n"
                 "    End()\n",
                 x, y, 10200 + (id % 1000), x, y);
    } else {
        return 0;
    }
    return dst[0] != '\0';
}
