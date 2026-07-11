#include "ui.h"

static const UIIconType ui_profile_picture_icons[] = {
    UI_ICON_TYPE_PFP_BIRD,
    UI_ICON_TYPE_PFP_BOWL,
    UI_ICON_TYPE_PFP_CACTUS,
    UI_ICON_TYPE_PFP_HEART,
    UI_ICON_TYPE_PFP_INCENSE,
    UI_ICON_TYPE_PFP_LOTUS,
    UI_ICON_TYPE_PFP_TREE1,
    UI_ICON_TYPE_PFP_TREE2,
    UI_ICON_TYPE_PFP_TREE3,
    UI_ICON_TYPE_PFP_TREE4,
    UI_ICON_TYPE_PFP_TREE5
};

static const char *ui_profile_picture_names[] = {
    "pfp_bird",
    "pfp_bowl",
    "pfp_cactus",
    "pfp_heart",
    "pfp_incense",
    "pfp_lotus",
    "pfp_tree1",
    "pfp_tree2",
    "pfp_tree3",
    "pfp_tree4",
    "pfp_tree5"
};

int
GetUIProfilePictureIconCount(void)
{
    return (int)(sizeof(ui_profile_picture_icons) /
                 sizeof(ui_profile_picture_icons[0]));
}

UIIconType
GetUIProfilePictureIconType(int index)
{
    if(index < 0 || index >= GetUIProfilePictureIconCount())
        return UI_ICON_TYPE_NONE;
    return ui_profile_picture_icons[index];
}

const char *
GetUIProfilePictureIconName(int index)
{
    if(index < 0 || index >= GetUIProfilePictureIconCount())
        return NULL;
    return ui_profile_picture_names[index];
}

static void
ui_draw_pfp_texture(Texture2D icon, int x, int y, int size)
{
    Rectangle src;
    Rectangle dst = {(float)x, (float)y, (float)size, (float)size};

    if(icon.id == 0 || size <= 0)
        return;
    src = (Rectangle){0.0f, 0.0f, (float)icon.width, (float)icon.height};
    DrawTexturePro(icon, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
}

static void
ui_draw_pfp_fallback(int x, int y, int size, Color color)
{
    int cx = x + size / 2;
    int head_r = size / 6;

    DrawCircle(cx, y + size / 3, (float)head_r, color);
    DrawCircle(cx, y + size * 2 / 3, (float)(size / 4), color);
    DrawRectangle(x + size / 4, y + size * 2 / 3, size / 2,
                  size / 5, color);
}

UISidebarAccountHeaderResult
DrawUISidebarAccountHeader(UISidebarAccountHeader header)
{
    UISidebarAccountHeaderResult result = {0};
    int height = header.height > 0 ? header.height : ScaleUIPx(138);
    int banner_h = ScaleUIPx(48);
    int avatar_r = ScaleUIPx(28);
    int avatar_size = avatar_r * 2;
    int avatar_x = header.x + ScaleUIPx(16) + avatar_r;
    int avatar_y = header.y + banner_h + ScaleUIPx(24);
    int name_x = avatar_x + avatar_r + ScaleUIPx(14);
    int name_y = avatar_y - ScaleUIPx(14);
    int name_font = GetUIFontSize();
    int small_font = GetUISmallFontSize();
    int count_y = avatar_y + avatar_r + ScaleUIPx(8);
    int click_enabled = header.current_frame == 0 ||
                        header.current_frame != header.block_click_frame;
    const char *username = header.username != NULL ? header.username : "";
    const char *subtitle = header.subtitle != NULL ? header.subtitle : "";
    const char *friends_text =
        header.friends_text != NULL ? header.friends_text : "";
    int username_w = MeasureUIText(username, name_font) + ScaleUIPx(8);
    int username_h = GetUITextHeight(username, name_font) + ScaleUIPx(8);
    int max_name_w = header.width - (name_x - header.x) - ScaleUIPx(12);
    Texture2D pfp_icon = header.pfp_icon;
    Rectangle pfp_bounds;
    Rectangle username_bounds;
    Rectangle friends_bounds;
    Vector2 mouse = ui_mouse_world();
    int released = click_enabled && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

    if(max_name_w < ScaleUIPx(48))
        max_name_w = ScaleUIPx(48);
    if(username_w > max_name_w)
        username_w = max_name_w;
    if(pfp_icon.id == 0 && header.icons != NULL &&
       header.pfp_icon_type > UI_ICON_TYPE_NONE &&
       header.pfp_icon_type < UI_ICON_TYPE_COUNT)
        pfp_icon = header.icons[header.pfp_icon_type];

    pfp_bounds = (Rectangle){(float)(avatar_x - avatar_r - ScaleUIPx(4)),
                             (float)(avatar_y - avatar_r - ScaleUIPx(4)),
                             (float)(avatar_size + ScaleUIPx(8)),
                             (float)(avatar_size + ScaleUIPx(8))};
    username_bounds = (Rectangle){(float)name_x,
                                  (float)(name_y - ScaleUIPx(4)),
                                  (float)username_w, (float)username_h};
    friends_bounds = (Rectangle){(float)header.x, (float)count_y,
                                 (float)header.width, (float)ScaleUIPx(36)};
    result.height = height;

    DrawRectangleRounded((Rectangle){(float)header.x, (float)header.y,
                                     (float)header.width, (float)height},
                         0.06f, 8, DarkenUIColor(c_surface, 6));
    DrawRectangleRounded((Rectangle){(float)header.x, (float)header.y,
                                     (float)header.width, (float)banner_h},
                         0.06f, 8, DarkenUIColor(c_button, 12));
    DrawRectangle(header.x, header.y + banner_h - ScaleUIPx(12),
                  header.width, ScaleUIPx(12), DarkenUIColor(c_button, 12));

    if(CheckCollisionPointRec(mouse, pfp_bounds) && !UIInputCapturesClick(mouse)) {
        MarkUIClickable();
        if(released)
            result.pfp_clicked = 1;
    }
    DrawCircle(avatar_x, avatar_y, (float)(avatar_r + ScaleUIPx(3)), c_surface);
    DrawCircle(avatar_x, avatar_y, (float)avatar_r,
               LightenUIColor(c_surface, 12));
    DrawCircleLines(avatar_x, avatar_y, (float)avatar_r,
                    DarkenUIColor(c_text, 38));
    if(pfp_icon.id != 0)
        ui_draw_pfp_texture(pfp_icon, avatar_x - avatar_r,
                            avatar_y - avatar_r, avatar_size);
    else
        ui_draw_pfp_fallback(avatar_x - avatar_r, avatar_y - avatar_r,
                             avatar_size, c_icon);

    if(CheckCollisionPointRec(mouse, username_bounds) &&
       !UIInputCapturesClick(mouse)) {
        MarkUIClickable();
        if(released)
            result.username_clicked = 1;
    }
    DrawFittedUITextInRect(username, username_bounds, name_font,
                           UI_TEXT_8, c_text);
    if(subtitle[0] != '\0')
        DrawUIText(subtitle, name_x, name_y + ScaleUIPx(22), small_font,
                   DarkenUIColor(c_text, 34));

    if(CheckCollisionPointRec(mouse, friends_bounds) &&
       !UIInputCapturesClick(mouse)) {
        DrawRectangleRounded(friends_bounds, 0.18f, 8,
                             LightenUIColor(c_surface, 8));
        MarkUIClickable();
        if(released)
            result.friends_clicked = 1;
    }
    if(friends_text[0] != '\0')
        DrawUIText(friends_text, header.x + ScaleUIPx(12),
                   count_y + ScaleUIPx(8), small_font, c_text);

    return result;
}

UIProfilePicturePickerResult
DrawUIProfilePicturePickerModal(UIProfilePicturePickerModal modal)
{
    UIProfilePicturePickerResult result = {0};
    int count = GetUIProfilePictureIconCount();
    int width = modal.max_width > 0 ? ScaleUIPx(modal.max_width) : ScaleUIPx(360);
    int cell = ScaleUIPx(68);
    int gap = ScaleUIPx(10);
    int columns;
    int rows;
    int height;
    UIPanelFrame frame;
    UIIconType selected =
        modal.selected_icon_type != NULL ? *modal.selected_icon_type
                                         : UI_ICON_TYPE_NONE;
    Vector2 mouse;

    if(width > ui_view_width - ScaleUIPx(24))
        width = ui_view_width - ScaleUIPx(24);
    if(width < ScaleUIPx(240))
        width = ScaleUIPx(240);
    columns = (width - ScaleUIPx(36) + gap) / (cell + gap);
    if(columns < 2)
        columns = 2;
    if(columns > count)
        columns = count;
    rows = (count + columns - 1) / columns;
    height = ScaleUIPx(74) + rows * cell + (rows - 1) * gap + ScaleUIPx(18);

    frame = DrawUIModalFrame(width, height,
                             modal.title != NULL ? modal.title : "Profile picture",
                             (Texture2D){0}, modal.close_icon);
    if(frame.right_clicked) {
        result.closed = 1;
        return result;
    }

    mouse = ui_mouse_world();
    for(int i = 0; i < count; i++) {
        int row = i / columns;
        int col = i % columns;
        int grid_w = columns * cell + (columns - 1) * gap;
        int x = frame.content_x + (frame.content_w - grid_w) / 2 +
                col * (cell + gap);
        int y = frame.content_y + row * (cell + gap);
        UIIconType type = GetUIProfilePictureIconType(i);
        Texture2D icon = {0};
        Rectangle bounds = {(float)x, (float)y, (float)cell, (float)cell};
        int hovered = CheckCollisionPointRec(mouse, bounds) &&
                      !UIInputCapturesClick(mouse);
        int active = type == selected;

        DrawRectangleRounded(bounds, 0.12f, 8,
                             hovered ? LightenUIColor(c_surface, 10)
                                     : c_surface);
        DrawUIBevel(x, y, cell, cell,
                    LightenUIColor(c_surface, active ? 68 : 34),
                    DarkenUIColor(c_surface, active ? 68 : 34));
        if(active)
            DrawRectangleLinesEx(bounds, ScaleUIPx(2), c_button_hover);
        if(modal.icons != NULL && type > UI_ICON_TYPE_NONE &&
           type < UI_ICON_TYPE_COUNT)
            icon = modal.icons[type];
        if(icon.id != 0)
            ui_draw_pfp_texture(icon, x + ScaleUIPx(10), y + ScaleUIPx(10),
                                cell - ScaleUIPx(20));
        else
            ui_draw_pfp_fallback(x + ScaleUIPx(10), y + ScaleUIPx(10),
                                 cell - ScaleUIPx(20), c_icon);

        if(hovered) {
            MarkUIClickable();
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                if(modal.selected_icon_type != NULL)
                    *modal.selected_icon_type = type;
                result.changed = type != selected;
                result.selected_index = i;
                result.selected_icon_type = type;
                result.closed = 1;
            }
        }
    }

    return result;
}
