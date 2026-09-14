#include "ui_internal.h"
#include "ui_image_internal.h"
#include "ui_style_internal.h"
#include "runtime/input.h"
#include "runtime/profile_header.h"

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const Texture2D kryon_zero_texture2d;


static const IconType ui_profile_image_icons[] = {
    ICON_PFP_BAMBUS,
    ICON_PFP_BIRD,
    ICON_PFP_BOWL,
    ICON_PFP_BUSH,
    ICON_PFP_BUTTERFLY,
    ICON_PFP_CACTUS,
    ICON_PFP_COFFEE,
    ICON_PFP_DRAGONFLY,
    ICON_PFP_FIREPLACE,
    ICON_PFP_FLOWER1,
    ICON_PFP_FLOWER2,
    ICON_PFP_FOX,
    ICON_PFP_HEART,
    ICON_PFP_INCENSE,
    ICON_PFP_LOTUS,
    ICON_PFP_MOUNTAIN,
    ICON_PFP_MUSHROOM,
    ICON_PFP_PALM,
    ICON_PFP_PERSON1,
    ICON_PFP_RAINBOW,
    ICON_PFP_TENT,
    ICON_PFP_TREE1,
    ICON_PFP_TREE2,
    ICON_PFP_TREE3,
    ICON_PFP_TREE4
};

static const int ui_profile_image_sync_ids[] = {
    SYNC_PROFILE_ICON_BAMBUS,
    SYNC_PROFILE_ICON_BIRD,
    SYNC_PROFILE_ICON_BOWL,
    SYNC_PROFILE_ICON_BUSH,
    SYNC_PROFILE_ICON_BUTTERFLY,
    SYNC_PROFILE_ICON_CACTUS,
    SYNC_PROFILE_ICON_COFFEE,
    SYNC_PROFILE_ICON_DRAGONFLY,
    SYNC_PROFILE_ICON_FIREPLACE,
    SYNC_PROFILE_ICON_FLOWER1,
    SYNC_PROFILE_ICON_FLOWER2,
    SYNC_PROFILE_ICON_FOX,
    SYNC_PROFILE_ICON_HEART,
    SYNC_PROFILE_ICON_INCENSE,
    SYNC_PROFILE_ICON_LOTUS,
    SYNC_PROFILE_ICON_MOUNTAIN,
    SYNC_PROFILE_ICON_MUSHROOM,
    SYNC_PROFILE_ICON_PALM,
    SYNC_PROFILE_ICON_PERSON1,
    SYNC_PROFILE_ICON_RAINBOW,
    SYNC_PROFILE_ICON_TENT,
    SYNC_PROFILE_ICON_TREE1,
    SYNC_PROFILE_ICON_TREE2,
    SYNC_PROFILE_ICON_TREE3,
    SYNC_PROFILE_ICON_TREE4
};

static const char *ui_profile_image_names[] = {
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
GetProfileImageIconCount(void)
{
    return (int)(sizeof(ui_profile_image_icons) /
                 sizeof(ui_profile_image_icons[0]));
}

IconType
GetProfileImageIconType(int index)
{
    if(index < 0 || index >= GetProfileImageIconCount())
        return ICON_NONE;
    return ui_profile_image_icons[index];
}

const char *
GetProfileImageIconName(int index)
{
    if(index < 0 || index >= GetProfileImageIconCount())
        return NULL;
    return ui_profile_image_names[index];
}

IconType
GetProfileImageIconTypeForSyncID(int sync_id)
{
    int i;

    if(sync_id == SYNC_PROFILE_ICON_NONE)
        return ICON_NONE;
    for(i = 0; i < GetProfileImageIconCount(); i++) {
        if(ui_profile_image_sync_ids[i] == sync_id)
            return ui_profile_image_icons[i];
    }
    return ICON_NONE;
}

int
GetSyncIDForProfileImageIconType(IconType type)
{
    int i;

    if(type == ICON_NONE)
        return SYNC_PROFILE_ICON_NONE;
    for(i = 0; i < GetProfileImageIconCount(); i++) {
        if(ui_profile_image_icons[i] == type)
            return ui_profile_image_sync_ids[i];
    }
    return SYNC_PROFILE_ICON_NONE;
}

static void
ui_draw_pfp_texture(Texture2D icon, int x, int y, int size)
{
    ImageProps image = {0};

    if(icon.id == 0 || size <= 0)
        return;
    image.bounds = (Rectangle){(float)x, (float)y, (float)size, (float)size};
    image.fit = ImageFitStretch;
    ImageTextureTinted(icon, image, WHITE);
}

static void
ui_draw_pfp_texture_in_circle(Texture2D icon, int cx, int cy, int radius)
{
    int inner_size = radius * 2;
    int x;
    int y;

    x = cx - inner_size / 2;
    y = cy - inner_size / 2;
    ui_draw_pfp_texture(icon, x, y, inner_size);
}

static void
ui_draw_avatar_tile(Rectangle bounds, Color background, Color outline)
{
    ui_draw_control_background(bounds, background, outline, 0.22f);
}

static int
ui_profile_images_dark_mode(void)
{
    return IsThemeColorDark(ui_surface_style().background) ? 1 : 0;
}

static InputPointerInteraction
ui_profile_pointer_interaction(Rectangle bounds, Vector2 mouse, int released)
{
    return InputPointerInteractionFor(
        CheckCollisionPointRec(mouse, bounds) != 0,
        InputCapturesClick(mouse) != 0, false, HoverEffectsEnabled() != 0,
        released != 0, false, true);
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

SidebarAccountHeaderResult
RenderSidebarAccountHeader(SidebarAccountHeaderProps header)
{
    SidebarAccountHeaderResult result = {0};
    int name_font = GetFontSize();
    int small_font = GetSmallFontSize();
    int click_enabled = header.current_frame == 0 ||
                        header.current_frame != header.block_click_frame;
    const char *username = header.username != NULL ? header.username : "";
    const char *subtitle = header.subtitle != NULL ? header.subtitle : "";
    const char *friends_text =
        header.friends_text != NULL ? header.friends_text : "";
    float scale = (float)GetScale();
    int username_w =
        ProfileHeaderUsernameHitWidth(TextWidth(username, name_font), scale);
    int username_h =
        ProfileHeaderUsernameHitHeight(TextHeight(username, name_font), scale);
    ProfileHeaderLayout layout;
    Texture2D pfp_icon = header.pfp_icon;
    Vector2 mouse = ui_mouse_world();
    int released = click_enabled && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

    layout = ProfileHeaderLayoutFor(header.x, header.y, header.width,
                                    header.height, header.content_padding_x,
                                    username_w, username_h, scale);
    if(pfp_icon.id == 0 && header.icons != NULL &&
       header.pfp_icon_type > ICON_NONE &&
       header.pfp_icon_type < ICON_COUNT)
        pfp_icon = header.icons[header.pfp_icon_type];

    result.height = layout.height;
    Style surface_style = ui_surface_style();
    Style text_style = ui_resolve_button_style_kind((ButtonProps){0},
                                                    ButtonStateNormal,
                                                    StyleKindText());
    Style hover_style = ui_resolve_button_style_kind(
        (ButtonProps){.tone = ButtonToneNeutral,
                      .emphasis = ButtonEmphasisSoft},
                                                    ButtonStateHover, StyleKindSelectable());
    Color muted_text = text_style.foreground;
    muted_text.a = (unsigned char)(muted_text.a * 0.72f);
    ui_draw_material(layout.header_bounds, (Rectangle){0}, surface_style.background,
                     surface_style.border, surface_style.border,
                     surface_style.radius, surface_style.border_width,
                     0.0f, 0.0f, 0, surface_style.focus, 0.0f,
                     surface_style.opacity, ui_style_fill(surface_style),
                     surface_style.material);
    InputPointerInteraction profile_interaction =
        ui_profile_pointer_interaction(layout.profile_bounds, mouse, released);
    if(profile_interaction.active) {
        MarkClickable();
        if(profile_interaction.activated) {
            ConsumeRelease();
            result.pfp_clicked = 1;
        }
    }
    ui_draw_avatar_tile(layout.avatar_tile_bounds,
                        surface_style.background, surface_style.border);
    if(header.pfp_icon_type > ICON_NONE &&
       header.pfp_icon_type < ICON_COUNT) {
        DrawProfileImageIcon(header.pfp_icon_type, layout.icon_bounds,
                               ui_profile_images_dark_mode());
    } else if(pfp_icon.id != 0)
        ui_draw_pfp_texture_in_circle(pfp_icon, layout.avatar_center_x,
                                      layout.avatar_center_y,
                                      layout.avatar_radius);
    else
        ui_draw_pfp_fallback(layout.avatar_center_x - layout.avatar_radius,
                             layout.avatar_center_y - layout.avatar_radius,
                             layout.avatar_size, text_style.foreground);

    InputPointerInteraction username_interaction =
        ui_profile_pointer_interaction(layout.username_bounds, mouse, released);
    if(username_interaction.active) {
        MarkClickable();
        if(username_interaction.activated) {
            ConsumeRelease();
            result.username_clicked = 1;
        }
    }
    DrawFittedTextInRect(username, layout.username_bounds, name_font,
                           Text8, text_style.foreground);
    if(subtitle[0] != '\0')
        RenderText(subtitle, layout.name_x,
                   ProfileHeaderSubtitleY(layout.name_y, scale), small_font,
                   muted_text);

    InputPointerInteraction friends_interaction =
        ui_profile_pointer_interaction(layout.friends_bounds, mouse, released);
    if(friends_interaction.active) {
        ui_draw_material(layout.friends_bounds, layout.header_bounds,
                         hover_style.background, hover_style.border,
                         hover_style.border, hover_style.radius,
                         hover_style.border_width, 1.0f, 0.0f, 0,
                         hover_style.focus, 0.0f, hover_style.opacity,
                         ui_style_fill(hover_style), hover_style.material);
        MarkClickable();
        if(friends_interaction.activated) {
            ConsumeRelease();
            result.friends_clicked = 1;
        }
    }
    if(friends_text[0] != '\0')
        RenderText(friends_text, ProfileHeaderFriendsTextX(header.x, scale),
                   ProfileHeaderFriendsTextY(layout.count_y, scale), small_font,
                   text_style.foreground);

    return result;
}

ProfileImagePickerResult
RenderProfileImagePickerModal(ProfileImagePickerProps modal)
{
    ProfileImagePickerResult result = {0};
    static int default_scroll_offset = 0;
    int count = GetProfileImageIconCount();
    ProfilePickerLayout layout;
    int *scroll_offset = modal.scroll_offset != NULL ? modal.scroll_offset
                                                     : &default_scroll_offset;
    ScrollArea scroll_area;
    ScrollView scroll_view;
    PanelFrame frame;
    IconType selected =
        modal.selected_icon_type != NULL ? *modal.selected_icon_type
                                         : ICON_NONE;
    Vector2 mouse;
    int i;

    layout = ProfilePickerLayoutFor(ui_view_width, ui_view_height,
                                    modal.max_width, count,
                                    (float)GetScale());

    frame = RenderModalFrame(layout.width, layout.height,
                             modal.title != NULL ? modal.title : "Profile image",
                             kryon_zero_texture2d, modal.close_icon);
    if(frame.right_clicked) {
        result.closed = 1;
        return result;
    }

    memset(&scroll_area, 0, sizeof(scroll_area));
    scroll_area.bounds.x = (float)frame.content_x;
    scroll_area.bounds.y = (float)frame.content_y;
    scroll_area.bounds.width = (float)frame.content_w;
    scroll_area.bounds.height = (float)frame.content_h;
    scroll_area.content_height = layout.content_height;
    scroll_area.content_x = frame.content_x;
    scroll_area.content_width = frame.content_w;
    scroll_area.scroll_offset = scroll_offset;
    scroll_area.wheel_step = layout.cell + layout.gap;
    scroll_view = BeginScrollContainer(scroll_area);

    mouse = ui_mouse_world();
    for(i = 0; i < count; i++) {
        ProfilePickerCell cell =
            ProfilePickerCellFor(i, scroll_view.content_x,
                                 scroll_view.content_y,
                                 scroll_view.content_w, layout);
        IconType type = GetProfileImageIconType(i);
        Texture2D icon = {0};
        int hovered = CheckCollisionPointRec(mouse, cell.bounds) &&
                      !InputCapturesClick(mouse);
        int active = type == selected;
        ButtonState state = active ? ButtonStateSelected
                          : hovered ? ButtonStateHover
                          : ButtonStateNormal;
        Style cell_style = ui_resolve_button_style_kind(
            (ButtonProps){.tone = active ? ButtonToneAccent : ButtonToneNeutral,
                          .emphasis = ButtonEmphasisSoft,
                          .selected = active},
            state, StyleKindSelectable());
        Style text_style = ui_resolve_button_style_kind((ButtonProps){0},
                                                        ButtonStateNormal,
                                                        StyleKindText());

        ui_draw_avatar_tile(cell.bounds,
                            cell_style.background, cell_style.border);
        if(active)
            DrawRectangleLinesEx(cell.bounds, Scale(2), cell_style.border);
        if(modal.icons != NULL && type > ICON_NONE &&
           type < ICON_COUNT)
            icon = modal.icons[type];
        if(type > ICON_NONE && type < ICON_COUNT) {
            DrawProfileImageIcon(type, cell.icon_bounds,
                                   ui_profile_images_dark_mode());
        } else if(icon.id != 0)
            ui_draw_pfp_texture(icon, (int)cell.icon_bounds.x,
                                (int)cell.icon_bounds.y,
                                (int)cell.icon_bounds.width);
        else
            ui_draw_pfp_fallback((int)cell.icon_bounds.x,
                                 (int)cell.icon_bounds.y,
                                 (int)cell.icon_bounds.width,
                                 text_style.foreground);

        if(hovered) {
            MarkClickable();
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                ConsumeRelease();
                if(modal.selected_icon_type != NULL)
                    *modal.selected_icon_type = type;
                result.changed = type != selected;
                result.selected_index = i;
                result.selected_icon_type = type;
                result.closed = 1;
            }
        }
    }
    EndScrollContainer(scroll_area, scroll_view);

    return result;
}
