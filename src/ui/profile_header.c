#include "ui_internal.h"

static const UIIconType ui_profile_picture_icons[] = {
    UI_ICON_TYPE_PFP_BAMBUS,
    UI_ICON_TYPE_PFP_BIRD,
    UI_ICON_TYPE_PFP_BOWL,
    UI_ICON_TYPE_PFP_BUSH,
    UI_ICON_TYPE_PFP_BUTTERFLY,
    UI_ICON_TYPE_PFP_CACTUS,
    UI_ICON_TYPE_PFP_COFFEE,
    UI_ICON_TYPE_PFP_DRAGONFLY,
    UI_ICON_TYPE_PFP_FIREPLACE,
    UI_ICON_TYPE_PFP_FLOWER1,
    UI_ICON_TYPE_PFP_FLOWER2,
    UI_ICON_TYPE_PFP_FOX,
    UI_ICON_TYPE_PFP_HEART,
    UI_ICON_TYPE_PFP_INCENSE,
    UI_ICON_TYPE_PFP_LOTUS,
    UI_ICON_TYPE_PFP_MOUNTAIN,
    UI_ICON_TYPE_PFP_MUSHROOM,
    UI_ICON_TYPE_PFP_PALM,
    UI_ICON_TYPE_PFP_PERSON1,
    UI_ICON_TYPE_PFP_RAINBOW,
    UI_ICON_TYPE_PFP_TENT,
    UI_ICON_TYPE_PFP_TREE1,
    UI_ICON_TYPE_PFP_TREE2,
    UI_ICON_TYPE_PFP_TREE3,
    UI_ICON_TYPE_PFP_TREE4
};

static const int ui_profile_picture_lyra_ids[] = {
    UI_LYRA_PROFILE_ICON_BAMBUS,
    UI_LYRA_PROFILE_ICON_BIRD,
    UI_LYRA_PROFILE_ICON_BOWL,
    UI_LYRA_PROFILE_ICON_BUSH,
    UI_LYRA_PROFILE_ICON_BUTTERFLY,
    UI_LYRA_PROFILE_ICON_CACTUS,
    UI_LYRA_PROFILE_ICON_COFFEE,
    UI_LYRA_PROFILE_ICON_DRAGONFLY,
    UI_LYRA_PROFILE_ICON_FIREPLACE,
    UI_LYRA_PROFILE_ICON_FLOWER1,
    UI_LYRA_PROFILE_ICON_FLOWER2,
    UI_LYRA_PROFILE_ICON_FOX,
    UI_LYRA_PROFILE_ICON_HEART,
    UI_LYRA_PROFILE_ICON_INCENSE,
    UI_LYRA_PROFILE_ICON_LOTUS,
    UI_LYRA_PROFILE_ICON_MOUNTAIN,
    UI_LYRA_PROFILE_ICON_MUSHROOM,
    UI_LYRA_PROFILE_ICON_PALM,
    UI_LYRA_PROFILE_ICON_PERSON1,
    UI_LYRA_PROFILE_ICON_RAINBOW,
    UI_LYRA_PROFILE_ICON_TENT,
    UI_LYRA_PROFILE_ICON_TREE1,
    UI_LYRA_PROFILE_ICON_TREE2,
    UI_LYRA_PROFILE_ICON_TREE3,
    UI_LYRA_PROFILE_ICON_TREE4
};

static const char *ui_profile_picture_names[] = {
    "pfp_bambus",
    "pfp_bird",
    "pfp_bowl",
    "pfp_bush",
    "pfp_butterfly",
    "pfp_cactus",
    "pfp_coffee",
    "pfp_dragonfly",
    "pfp_fireplace",
    "pfp_flower1",
    "pfp_flower2",
    "pfp_fox",
    "pfp_heart",
    "pfp_incense",
    "pfp_lotus",
    "pfp_mountain",
    "pfp_mushroom",
    "pfp_palm",
    "pfp_person1",
    "pfp_rainbow",
    "pfp_tent",
    "pfp_tree1",
    "pfp_tree2",
    "pfp_tree3",
    "pfp_tree4"
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

UIIconType
GetUIProfilePictureIconTypeForLyraID(int lyra_id)
{
    if(lyra_id == UI_LYRA_PROFILE_ICON_NONE)
        return UI_ICON_TYPE_NONE;
    for(int i = 0; i < GetUIProfilePictureIconCount(); i++) {
        if(ui_profile_picture_lyra_ids[i] == lyra_id)
            return ui_profile_picture_icons[i];
    }
    return UI_ICON_TYPE_NONE;
}

int
GetUILyraIDForProfilePictureIconType(UIIconType type)
{
    if(type == UI_ICON_TYPE_NONE)
        return UI_LYRA_PROFILE_ICON_NONE;
    for(int i = 0; i < GetUIProfilePictureIconCount(); i++) {
        if(ui_profile_picture_icons[i] == type)
            return ui_profile_picture_lyra_ids[i];
    }
    return UI_LYRA_PROFILE_ICON_NONE;
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
ui_draw_pfp_texture_in_circle(Texture2D icon, int cx, int cy, int radius)
{
    int inner_size = radius * 14 / 10;
    int x;
    int y;

    if(inner_size > radius * 2)
        inner_size = radius * 2;
    x = cx - inner_size / 2;
    y = cy - inner_size / 2;
    ui_draw_pfp_texture(icon, x, y, inner_size);
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
    int top_pad = ScaleUIPx(24);
    int avatar_r = ScaleUIPx(28);
    int avatar_size = avatar_r * 2;
    int padding_x = header.content_padding_x > 0 ? header.content_padding_x
                                                     : ScaleUIPx(16);
    int avatar_x = header.x + padding_x + avatar_r;
    int avatar_y = header.y + top_pad + avatar_r;
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
    if(CheckCollisionPointRec(mouse, pfp_bounds) && !UIInputCapturesClick(mouse)) {
        MarkUIClickable();
        if(released) {
            UIConsumeRelease();
            result.pfp_clicked = 1;
        }
    }
    DrawCircle(avatar_x, avatar_y, (float)(avatar_r + ScaleUIPx(3)), c_surface);
    DrawCircle(avatar_x, avatar_y, (float)avatar_r,
               LightenUIColor(c_surface, 12));
    DrawCircleLines(avatar_x, avatar_y, (float)avatar_r,
                    DarkenUIColor(c_text, 38));
    if(pfp_icon.id != 0)
        ui_draw_pfp_texture_in_circle(pfp_icon, avatar_x, avatar_y, avatar_r);
    else
        ui_draw_pfp_fallback(avatar_x - avatar_r, avatar_y - avatar_r,
                             avatar_size, c_icon);

    if(CheckCollisionPointRec(mouse, username_bounds) &&
       !UIInputCapturesClick(mouse)) {
        MarkUIClickable();
        if(released) {
            UIConsumeRelease();
            result.username_clicked = 1;
        }
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
        if(released) {
            UIConsumeRelease();
            result.friends_clicked = 1;
        }
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
    static int default_scroll_offset = 0;
    int count = GetUIProfilePictureIconCount();
    int width = modal.max_width > 0 ? ScaleUIPx(modal.max_width) : ScaleUIPx(520);
    int gap = ScaleUIPx(8);
    int columns;
    int content_w;
    int cell;
    int rows;
    int grid_w;
    int content_h;
    int height;
    int max_height;
    int icon_inset;
    int *scroll_offset = modal.scroll_offset != NULL ? modal.scroll_offset
                                                     : &default_scroll_offset;
    UIScrollArea scroll_area;
    UIScrollView scroll_view;
    UIPanelFrame frame;
    UIIconType selected =
        modal.selected_icon_type != NULL ? *modal.selected_icon_type
                                         : UI_ICON_TYPE_NONE;
    Vector2 mouse;

    if(width > ui_view_width - ScaleUIPx(24))
        width = ui_view_width - ScaleUIPx(24);
    if(width < ScaleUIPx(240))
        width = ScaleUIPx(240);

    content_w = width - ScaleUIPx(36);
    columns = (content_w + gap) / (ScaleUIPx(64) + gap);
    if(columns < 3)
        columns = 3;
    if(columns > count)
        columns = count;
    if(columns < 1)
        columns = 1;
    cell = (content_w - (columns - 1) * gap) / columns;
    if(cell > ScaleUIPx(64))
        cell = ScaleUIPx(64);
    if(cell < ScaleUIPx(52))
        cell = ScaleUIPx(52);
    grid_w = columns * cell + (columns - 1) * gap;
    rows = columns > 0 ? (count + columns - 1) / columns : 0;
    content_h = rows > 0 ? rows * cell + (rows - 1) * gap : 0;
    height = ScaleUIPx(74) + content_h + ScaleUIPx(18);
    max_height = ui_view_height - ScaleUIPx(24);
    if(height > max_height)
        height = max_height;

    frame = DrawUIModalFrame(width, height,
                             modal.title != NULL ? modal.title : "Profile picture",
                             (Texture2D){0}, modal.close_icon);
    if(frame.right_clicked) {
        result.closed = 1;
        return result;
    }

    scroll_area = (UIScrollArea){
        .bounds = {(float)frame.content_x, (float)frame.content_y,
                   (float)frame.content_w, (float)frame.content_h},
        .content_height = content_h,
        .content_x = frame.content_x,
        .content_width = frame.content_w,
        .scroll_offset = scroll_offset,
        .wheel_step = cell + gap
    };
    scroll_view = BeginUIScrollContainer(scroll_area);

    mouse = ui_mouse_world();
    icon_inset = ScaleUIPx(12);
    for(int i = 0; i < count; i++) {
        int row = i / columns;
        int col = i % columns;
        int x = scroll_view.content_x + (scroll_view.content_w - grid_w) / 2 +
                col * (cell + gap);
        int y = scroll_view.content_y + row * (cell + gap);
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
            ui_draw_pfp_texture(icon, x + icon_inset, y + icon_inset,
                                cell - icon_inset * 2);
        else
            ui_draw_pfp_fallback(x + icon_inset, y + icon_inset,
                                 cell - icon_inset * 2, c_icon);

        if(hovered) {
            MarkUIClickable();
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                UIConsumeRelease();
                if(modal.selected_icon_type != NULL)
                    *modal.selected_icon_type = type;
                result.changed = type != selected;
                result.selected_index = i;
                result.selected_icon_type = type;
            }
        }
    }
    EndUIScrollContainer(scroll_area, scroll_view);

    return result;
}
