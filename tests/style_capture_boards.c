#include "kryon.h"
#include "kry_inject.h"
#include "ui_internal.h"
#include "ui_tree.h"
#include "ui_style_sheet.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Style capture boards (plan/style/06): one board per built-in pack plus a
   no-style board, so pack coverage and state styling stay visually
   reviewable. States are set through props, not input injection, so every
   board renders the same interaction-free specimen. */

static const char *const board_packs[] = {
    "material", "tk", "lightfield", "none",
};

static void
board_mkdir(const char *path)
{
    char tmp[1024];

    snprintf(tmp, sizeof(tmp), "%s", path);
    for(char *p = tmp + 1; *p != '\0'; p++) {
        if(*p != '/')
            continue;
        *p = '\0';
        mkdir(tmp, 0755);
        *p = '/';
    }
    mkdir(tmp, 0755);
}

static void
render_board(void)
{
    static char field_text[] = "style board";
    static int cursor = 6;
    static int dropdown_selected = 1;
    static int checkbox_flags = 5;
    static int toggle_on = 1;
    static const char *items[] = {"Morning", "Long Session", "Focus"};
    static const ButtonState states[] = {
        ButtonStateNormal, ButtonStateHover, ButtonStatePressed,
        ButtonStateFocus, ButtonStateDisabled, ButtonStateSelected,
        ButtonStateLoading,
    };
    static const char *state_names[] = {
        "normal", "hover", "pressed", "focus", "disabled", "selected",
        "loading",
    };
    static const ButtonTone tones[] = {ButtonToneNeutral, ButtonToneAccent,
                                       ButtonToneDanger};
    static const ButtonEmphasis emphases[] = {ButtonEmphasisFilled,
                                              ButtonEmphasisSoft,
                                              ButtonEmphasisOutline,
                                              ButtonEmphasisGhost};
    static const char *emphasis_names[] = {"filled", "soft", "outline",
                                           "ghost"};
    int y = 32;
    int i;
    int e;

    Text((TextProps){.bounds = {24, y, 640, 30},
        .text = "Kryon style capture board", .wrap = TextWrapNone});
    y += 44;

    Text((TextProps){.bounds = {24, y, 640, 22}, .text = "Button states",
        .wrap = TextWrapNone});
    y += 30;
    for(i = 0; i < (int)(sizeof(states) / sizeof(states[0])); i++) {
        Button((ButtonProps){.bounds = {24 + i * 124, y, 112, 36},
            .label = state_names[i], .id = 9000 + i, .state = states[i],
            .disabled = states[i] == ButtonStateDisabled,
            .loading = states[i] == ButtonStateLoading,
            .selected = states[i] == ButtonStateSelected,
            .tone = ButtonToneAccent, .emphasis = ButtonEmphasisFilled});
    }
    y += 52;

    Text((TextProps){.bounds = {24, y, 640, 22}, .text = "Tones x emphasis",
        .wrap = TextWrapNone});
    y += 30;
    for(i = 0; i < (int)(sizeof(tones) / sizeof(tones[0])); i++) {
        for(e = 0; e < (int)(sizeof(emphases) / sizeof(emphases[0])); e++) {
            Button((ButtonProps){.bounds = {24 + e * 160, y + i * 44, 148, 36},
                .label = emphasis_names[e], .id = 9100 + i * 10 + e,
                .tone = tones[i], .emphasis = emphases[e]});
        }
    }
    y += 3 * 44 + 18;
    Text((TextProps){.bounds = {24, y, 640, 22}, .text = "Class selector",
        .wrap = TextWrapNone});
    y += 30;
    Button((ButtonProps){.bounds = {24, y, 220, 36}, .label = ".primary",
        .id = 9200, .class_name = StyleClassId("primary"),
        .tone = ButtonToneAccent, .emphasis = ButtonEmphasisFilled});
    y += 52;

    Text((TextProps){.bounds = {24, y, 640, 22}, .text = "TextInput",
        .wrap = TextWrapNone});
    y += 30;
    DrawTextInput((Rectangle){24, y, 320, 40}, field_text, cursor, 1, 1,
                  Text18, 9300, 0);
    y += 56;

    Text((TextProps){.bounds = {24, y, 640, 22}, .text = "Selection widgets",
        .wrap = TextWrapNone});
    y += 30;
    Checkbox((CheckboxProps){.bounds = {24, y, 200, 34}, .id = 9400,
        .label = "Feature", .flags = &checkbox_flags, .flags_value = 1});
    Checkbox((CheckboxProps){.bounds = {24, y + 42, 200, 34}, .id = 9401,
        .label = "Disabled", .flags = &checkbox_flags, .flags_value = 4,
        .disabled = 1});
    Toggle((ToggleProps){.bounds = {250, y, 220, 34}, .id = 9410,
        .value = &toggle_on, .off_label = "Off", .on_label = "On"});
    Progress((ProgressProps){.bounds = {250, y + 44, 220, 20}, .min = 0,
        .max = 100, .value = 62, .label = "Progress"});
    y += 96;

    Text((TextProps){.bounds = {24, y, 640, 22}, .text = "Dropdown",
        .wrap = TextWrapNone});
    y += 30;
    Dropdown((DropdownProps){.id = 9500, .bounds = {24, y, 260, 40},
        .options = items, .option_count = 3,
        .selected_index = &dropdown_selected});
}

int
main(int argc, char **argv)
{
    const char *out_dir = argc > 1 ? argv[1] : "build/linux-x86_64/style-boards";
    const int width = 900;
    const int height = 960;
    RenderTexture2D target;
    Image image;
    char out[1024];

    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_MSAA_4X_HINT);
    InitWindow(width, height, "Kryon style capture boards");
    SetTargetFPS(60);
    SetThemeMode(THEME_MODE_DARK);
    board_mkdir(out_dir);
    target = LoadRenderTexture(width, height);

    for(size_t i = 0; i < sizeof(board_packs) / sizeof(board_packs[0]); i++) {
        const char *pack = board_packs[i];

        if(strcmp(pack, "none") == 0)
            ClearStylePacks();
        else {
            EnsureBuiltInStylePacks();
            if(!SetActiveStylePack(pack)) {
                fprintf(stderr, "style pack '%s' unavailable\n", pack);
                continue;
            }
        }
        InjectReset();
        InjectMousePosition(-1000, -1000);
        InjectPump();
        BeginDrawing();
        BeginTextureMode(target);
        ClearBackground(GetThemeBackground());
        BeginInterfaceFrame(width, height, 1.0f);
        BeginTree(Key(pack));
        render_board();
        EndTree();
        EndInterfaceFrame();
        EndTextureMode();
        EndDrawing();

        image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        snprintf(out, sizeof(out), "%s/%s.png", out_dir, pack);
        if(!ExportImage(image, out))
            fprintf(stderr, "failed to write %s\n", out);
        else
            printf("%s\n", out);
        UnloadImage(image);
    }
    UnloadRenderTexture(target);
    CloseWindow();
    fflush(NULL);
    _Exit(0);
}
