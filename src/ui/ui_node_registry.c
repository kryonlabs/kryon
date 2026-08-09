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
    {"TextInRect", "Text In Rect", "UI/Display", "Control", "Wrapped label", EDITABLE},
    {"Paragraph", "Paragraph", "UI/Display", "Control", "Rich text", KRYON_NODE_SELECTABLE},
    {"TextLines", "Text Lines", "UI/Display", "Control", "Multi-line label", KRYON_NODE_SELECTABLE},
    {"Rect", "Rect", "UI/Display", "Control", "Shape", INSERT_EDITABLE},
    {"Line", "Line", "UI/Display", "Control", "Stroke", INSERT_EDITABLE},
    {"Bevel", "Bevel", "UI/Display", "Control", "Relief", EDITABLE},
    {"IconTexture", "Icon Texture", "UI/Display", "Control", "Icon", EDITABLE},
    {"Sprite", "Sprite", "UI/Display", "Control", "Image", INSERT_EDITABLE},

    {"Button", "Button", "UI/Input", "Control", "Action", INSERT_EDITABLE},
    {"IconButton", "Icon Button", "UI/Input", "Control", "Icon action", EDITABLE},
    {"Href", "Href", "UI/Input", "Control", "Link", EDITABLE},
    {"TextInputControl", "Text Input", "UI/Input", "Control", "Input", EDITABLE},
    {"GenericButton", "Generic Button", "UI/Input", "Control", "Action", EDITABLE},
    {"TextField", "Text Field", "UI/Input", "Control", "Input", INSERT_EDITABLE},
    {"ReadonlyTextBox", "Readonly Text Box", "UI/Input", "Control", "Read-only text", EDITABLE},
    {"IconBtn", "Icon Btn", "UI/Input", "Control", "Icon action", EDITABLE},
    {"PaddedIconBtn", "Padded Icon Btn", "UI/Input", "Control", "Icon action", EDITABLE},
    {"InfoButton", "Info Button", "UI/Input", "Control", "Help action", EDITABLE},
    {"TextButton", "Text Button", "UI/Input", "Control", "Text action", EDITABLE},
    {"IconLink", "Icon Link", "UI/Input", "Control", "Icon link", EDITABLE},
    {"Dropdown", "Dropdown", "UI/Input", "Control", "Selection", INSERT_EDITABLE},
    {"DropdownEx", "Dropdown Ex", "UI/Input", "Control", "Rich selection", EDITABLE},
    {"LocaleDropdown", "Locale Dropdown", "UI/Input", "Control", "Locale", EDITABLE},
    {"Slider", "Slider", "UI/Input", "Control", "Value", INSERT_EDITABLE},
    {"VerticalSlider", "Vertical Slider", "UI/Input", "Control", "Value", EDITABLE},
    {"VerticalSliderWithMarks", "Vertical Slider With Marks", "UI/Input", "Control", "Marked value", EDITABLE},
    {"Toggle", "Toggle", "UI/Input", "Control", "On/off", INSERT_EDITABLE},
    {"Checkbox", "Checkbox", "UI/Input", "Control", "Boolean", INSERT_EDITABLE},
    {"Radio", "Radio", "UI/Input", "Control", "Choice", EDITABLE},
    {"Progress", "Progress", "UI/Input", "Control", "Progress", EDITABLE},
    {"Spinbox", "Spinbox", "UI/Input", "Control", "Number", EDITABLE},
    {"Combobox", "Combobox", "UI/Input", "Control", "Selection", EDITABLE},
    {"ColorPicker", "Color Picker", "UI/Input", "Control", "Color", EDITABLE},

    {"Group", "Group", "UI/Layout", "Control", "Container", INSERT_EDITABLE},
    {"Separator", "Separator", "UI/Layout", "Control", "Divider", EDITABLE},
    {"LabelFrame", "Label Frame", "UI/Layout", "Control", "Frame", EDITABLE},
    {"Notebook", "Notebook", "UI/Layout", "Control", "Pages", EDITABLE},
    {"PanedView", "Paned View", "UI/Layout", "Control", "Split panes", EDITABLE},
    {"Collapsible", "Collapsible", "UI/Layout", "Control", "Section", EDITABLE},

    {"ListBox", "List Box", "UI/Collections", "Control", "List", EDITABLE},
    {"TreeView", "Tree View", "UI/Collections", "Control", "Tree", EDITABLE},
    {"CascadingTreeView", "Cascading Tree View", "UI/Collections", "Control", "Tree", EDITABLE},
    {"SourceView", "Source View", "UI/Collections", "Control", "Source", EDITABLE},
    {"TableView", "Table View", "UI/Collections", "Control", "Table", EDITABLE},
    {"TextArea", "Text Area", "UI/Collections", "Control", "Text area", EDITABLE},
    {"CanvasGrid", "Canvas Grid", "UI/Collections", "Control", "Grid", EDITABLE},

    {"MenuBar", "Menu Bar", "UI/Navigation", "Control", "Menu", EDITABLE},
    {"PopupMenu", "Popup Menu", "UI/Navigation", "Control", "Menu", EDITABLE},
    {"BottomNav", "Bottom Nav", "UI/Navigation", "Control", "Tabs", EDITABLE},
    {"TopNav", "Top Nav", "UI/Navigation", "Control", "Tabs", EDITABLE},
    {"Toolbar", "Toolbar", "UI/Navigation", "Control", "Tools", EDITABLE},
    {"ToolbarHeader", "Toolbar Header", "UI/Navigation", "Control", "Header tools", EDITABLE},
    {"SubtabBar", "Subtab Bar", "UI/Navigation", "Control", "Tabs", EDITABLE},
    {"TabBar", "Tab Bar", "UI/Navigation", "Control", "Tabs", EDITABLE},
    {"TitleBar", "Title Bar", "UI/Navigation", "Control", "Title", EDITABLE},
    {"ReturnTitleBar", "Return Title Bar", "UI/Navigation", "Control", "Title", EDITABLE},
    {"ReturnDropdownTitleBar", "Return Dropdown Title Bar", "UI/Navigation", "Control", "Title menu", EDITABLE},

    {"ThemeSettings", "Theme Settings", "UI/Overlays", "Control", "Theme", EDITABLE},
    {"ThemeSwitcher", "Theme Switcher", "UI/Overlays", "Control", "Theme", EDITABLE},
    {"ThemePicker", "Theme Picker", "UI/Overlays", "Control", "Theme", EDITABLE},
    {"ActionModal", "Action Modal", "UI/Overlays", "Control", "Dialog", EDITABLE},
    {"MessageDialog", "Message Dialog", "UI/Overlays", "Control", "Dialog", EDITABLE},
    {"ConfirmDialog", "Confirm Dialog", "UI/Overlays", "Control", "Dialog", EDITABLE},
    {"PromptDialog", "Prompt Dialog", "UI/Overlays", "Control", "Dialog", EDITABLE},
    {"Focus", "Focus", "UI/Overlays", "Control", "Focus", EDITABLE},
    {"FocusDebugOverlay", "Focus Debug Overlay", "UI/Overlays", "Control", "Debug", KRYON_NODE_SELECTABLE},
    {"GuideOverlay", "Guide Overlay", "UI/Overlays", "Control", "Guide", EDITABLE},
    {"TutorialImagePlaceholder", "Tutorial Image Placeholder", "UI/Overlays", "Control", "Image placeholder", EDITABLE},
    {"TutorialImage", "Tutorial Image", "UI/Overlays", "Control", "Image", EDITABLE},
    {"TransitionFade", "Transition Fade", "UI/Overlays", "Control", "Transition", EDITABLE},
    {"InfoRows", "Info Rows", "UI/Overlays", "Control", "Rows", EDITABLE},
    {"OverlayButton", "Overlay Button", "UI/Overlays", "Control", "Action", EDITABLE},
    {"IconSliderPopup", "Icon Slider Popup", "UI/Overlays", "Control", "Popup", EDITABLE},
    {"Modal", "Modal", "UI/Overlays", "Control", "Dialog", EDITABLE},
    {"Modal3Button", "Modal 3 Button", "UI/Overlays", "Control", "Dialog", EDITABLE},
    {"ModalFrame", "Modal Frame", "UI/Overlays", "Control", "Dialog frame", EDITABLE},

    {"LabelTextField", "Label Text Field", "UI/Composite", "Control", "Labeled input", EDITABLE},
    {"SectionLabel", "Section Label", "UI/Composite", "Control", "Heading", MOVABLE_TEXT},
    {"CheckboxRow", "Checkbox Row", "UI/Composite", "Control", "Boolean row", EDITABLE},
    {"ButtonRow", "Button Row", "UI/Composite", "Control", "Action row", EDITABLE},
    {"BottomIconRow", "Bottom Icon Row", "UI/Composite", "Control", "Icon row", EDITABLE},
    {"BottomNavConfig", "Bottom Nav Config", "UI/Composite", "Control", "Nav editor", EDITABLE},
    {"SidebarAccountHeader", "Sidebar Account Header", "UI/Composite", "Control", "Account", EDITABLE},
    {"ProfilePicturePicker", "Profile Picture Picker", "UI/Composite", "Control", "Avatar picker", EDITABLE},
    {"ReorderHandle", "Reorder Handle", "UI/Composite", "Control", "Drag handle", EDITABLE},
    {"ReorderPlaceholder", "Reorder Placeholder", "UI/Composite", "Control", "Drop target", EDITABLE},
    {"ImageBox", "Image Box", "UI/Composite", "Control", "Image", EDITABLE},

    {"Scene", "Scene", "Game2D/Core", "Node", "Scene root", 0},
    {"Node2D", "Node2D", "Game2D/Core", "Node", "Transform", 0},
    {"Camera2D", "Camera2D", "Game2D/Core", "Node2D", "Camera", 0},
    {"Sprite2D", "Sprite2D", "Game2D/Rendering", "Node2D", "Sprite", 0},
    {"AnimatedSprite2D", "AnimatedSprite2D", "Game2D/Rendering", "Node2D", "Animation", 0},
    {"TileMap", "TileMap", "Game2D/Rendering", "Node2D", "Tiles", 0},
    {"TileLayer", "TileLayer", "Game2D/Rendering", "Node2D", "Tile layer", 0},
    {"CollisionShape2D", "Collision Shape 2D", "Game2D/Physics", "Node2D", "Collider", 0},
    {"Area2D", "Area2D", "Game2D/Physics", "Node2D", "Trigger", 0},
    {"Body2D", "Body2D", "Game2D/Physics", "Node2D", "Body", 0},
    {"Timer", "Timer", "Game2D/Runtime", "Node", "Timer", 0},
    {"AudioSource", "Audio Source", "Game2D/Audio", "Node2D", "Sound", 0}
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
        "Sprite",
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
                 "\n    Text(\"Text\", ScaleUIPx(%d), ScaleUIPx(%d), UI_TEXT_16, GetThemeText())\n",
                 x, y);
    } else if(strcmp(type->name, "Rect") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    Rect(ScaleUIPx(%d), ScaleUIPx(%d), ScaleUIPx(160), ScaleUIPx(90), GetThemeButton(), GetThemeButtonHover())\n",
                 x, y);
    } else if(strcmp(type->name, "Line") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    Line(ScaleUIPx(%d), ScaleUIPx(%d), ScaleUIPx(%d), ScaleUIPx(%d), GetThemeLink())\n",
                 x, y, x + 160, y + 40);
    } else if(strcmp(type->name, "Sprite") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    Sprite((SpriteProps){\n"
                 "        .asset_path = \"assets/sprite.png\",\n"
                 "        .bounds = {ScaleUIPx(%d), ScaleUIPx(%d), ScaleUIPx(180), ScaleUIPx(110)},\n"
                 "        .tint = WHITE,\n"
                 "        .fit = UI_SPRITE_FIT_CONTAIN,\n"
                 "    })\n",
                 x, y);
    } else if(strcmp(type->name, "Button") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    if Button((ButtonProps){\n"
                 "        .bounds = {ScaleUIPx(%d), ScaleUIPx(%d), ScaleUIPx(140), ScaleUIPx(36)},\n"
                 "        .label = \"Button\",\n"
                 "        .style = UI_BUTTON_STYLE_PRIMARY,\n"
                 "        .font = UI_TEXT_16,\n"
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
                 "        .bounds = {ScaleUIPx(%d), ScaleUIPx(%d), ScaleUIPx(180), ScaleUIPx(34)},\n"
                 "        .text = field_%d,\n"
                 "        .text_size = sizeof(field_%d),\n"
                 "        .cursor_position = &field_cursor_%d,\n"
                 "        .focused = &field_focused_%d,\n"
                 "        .max_codepoints = 128,\n"
                 "        .font = UI_TEXT_16,\n"
                 "        .focus_id = %d,\n"
                 "        .style = (UITextInputStyle){\n"
                 "            GetThemeSurface(), GetThemeButton(), GetThemeLink(),\n"
                 "            GetThemeText(), GetThemeLink(), 0, ScaleUIPx(8), ScaleUIPx(6),\n"
                 "        },\n"
                 "    })\n",
                 id, id, id, x, y, id, id, id, id, 5200 + (id % 1000));
    } else if(strcmp(type->name, "Toggle") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    toggle_%d: int = 0\n"
                 "    Toggle(%d, ScaleUIPx(%d), ScaleUIPx(%d), ScaleUIPx(120), ScaleUIPx(28), &toggle_%d, \"Off\", \"On\")\n",
                 id, 6200 + (id % 1000), x, y, id);
    } else if(strcmp(type->name, "Slider") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    slider_%d: int = 50\n"
                 "    Slider(%d, ScaleUIPx(%d), ScaleUIPx(%d), ScaleUIPx(180), \"Value\", 0, 100, &slider_%d, \"\")\n",
                 id, 7200 + (id % 1000), x, y, id);
    } else if(strcmp(type->name, "Checkbox") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    check_%d: int = 0\n"
                 "    Checkbox(%d, ScaleUIPx(%d), ScaleUIPx(%d), \"Checkbox\", &check_%d)\n",
                 id, 8200 + (id % 1000), x, y, id);
    } else if(strcmp(type->name, "Dropdown") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    options_%d: [3] const char* = {\"One\", \"Two\", \"Three\"}\n"
                 "    selected_%d: int = 0\n"
                 "    Dropdown(%d, ScaleUIPx(%d), ScaleUIPx(%d), ScaleUIPx(180), ScaleUIPx(34), options_%d, 3, &selected_%d)\n",
                 id, id, 9200 + (id % 1000), x, y, id, id);
    } else if(strcmp(type->name, "Group") == 0) {
        snprintf(dst, (size_t)cap,
                 "\n    BeginNodeGroup(%d, (Rectangle){ScaleUIPx(%d), ScaleUIPx(%d), ScaleUIPx(180), ScaleUIPx(110)})\n"
                 "    Rect(ScaleUIPx(%d), ScaleUIPx(%d), ScaleUIPx(180), ScaleUIPx(110), Fade(GetThemeButton(), 0.45), GetThemeButtonHover())\n"
                 "    EndNodeGroup()\n",
                 10200 + (id % 1000), x, y, x, y);
    } else {
        return 0;
    }
    return dst[0] != '\0';
}
