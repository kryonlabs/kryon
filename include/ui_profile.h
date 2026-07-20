#ifndef UI_PROFILE_H
#define UI_PROFILE_H

#include "kryon_compat.generated.h"
#include "ui_icon_types.h"

typedef struct {
    int x;
    int y;
    int width;
    int height;
    const char *username;
    const char *subtitle;
    const char *friends_text;
    Texture2D pfp_icon;
    const Texture2D *icons;
    UIIconType pfp_icon_type;
    int content_padding_x;
    int current_frame;
    int block_click_frame;
} UISidebarAccountHeader;

typedef struct {
    int pfp_clicked;
    int username_clicked;
    int friends_clicked;
    int height;
} UISidebarAccountHeaderResult;

typedef struct {
    const char *title;
    const Texture2D *icons;
    UIIconType *selected_icon_type;
    Texture2D close_icon;
    int max_width;
    int *scroll_offset;
} UIProfilePicturePickerModal;

typedef struct {
    int closed;
    int changed;
    int selected_index;
    UIIconType selected_icon_type;
} UIProfilePicturePickerResult;

typedef enum {
    UI_LYRA_PROFILE_ICON_NONE = 0,
    UI_LYRA_PROFILE_ICON_BIRD = 1,
    UI_LYRA_PROFILE_ICON_BOWL = 2,
    UI_LYRA_PROFILE_ICON_CACTUS = 3,
    UI_LYRA_PROFILE_ICON_HEART = 4,
    UI_LYRA_PROFILE_ICON_INCENSE = 5,
    UI_LYRA_PROFILE_ICON_LOTUS = 6,
    UI_LYRA_PROFILE_ICON_TREE1 = 7,
    UI_LYRA_PROFILE_ICON_TREE2 = 8,
    UI_LYRA_PROFILE_ICON_TREE3 = 9,
    UI_LYRA_PROFILE_ICON_TREE4 = 10,
    UI_LYRA_PROFILE_ICON_TREE5 = 11,
    UI_LYRA_PROFILE_ICON_BAMBUS = 12,
    UI_LYRA_PROFILE_ICON_BUSH = 13,
    UI_LYRA_PROFILE_ICON_COFFEE = 14,
    UI_LYRA_PROFILE_ICON_FLOWER1 = 15,
    UI_LYRA_PROFILE_ICON_FLOWER2 = 16,
    UI_LYRA_PROFILE_ICON_MOUNTAIN = 17,
    UI_LYRA_PROFILE_ICON_MUSHROOM = 18,
    UI_LYRA_PROFILE_ICON_PERSON1 = 19,
    UI_LYRA_PROFILE_ICON_RAINBOW = 20,
    UI_LYRA_PROFILE_ICON_TENT = 21,
    UI_LYRA_PROFILE_ICON_BUTTERFLY = 22,
    UI_LYRA_PROFILE_ICON_DRAGONFLY = 23,
    UI_LYRA_PROFILE_ICON_FIREPLACE = 24,
    UI_LYRA_PROFILE_ICON_FOX = 25,
    UI_LYRA_PROFILE_ICON_PALM = 26
} UILyraProfileIcon;

int GetUIProfilePictureIconCount(void);
UIIconType GetUIProfilePictureIconType(int index);
const char *GetUIProfilePictureIconName(int index);
UIIconType GetUIProfilePictureIconTypeForLyraID(int lyra_id);
int GetUILyraIDForProfilePictureIconType(UIIconType type);
UISidebarAccountHeaderResult DrawUISidebarAccountHeader(UISidebarAccountHeader header);
UIProfilePicturePickerResult DrawUIProfilePicturePickerModal(UIProfilePicturePickerModal modal);

#endif
