#include "kryon.h"
#include "kry_inject.h"
#include <stdio.h>
#include <string.h>

/* Real renderer regression: retained content must be beneath an immediate
 * modal, and content declared after the modal must still paint above it. */
int main(void)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(640, 480, "overlay paint regression");
    if(!IsWindowReady()) return 1;
    RenderTexture2D target = LoadRenderTexture(640, 480);
    if(!target.id) return 1;
    int failures = 0;
    char text[32] = "Value";
    int cursor = 5, focused = 0;
    const char *choices[] = {"One", "Two"};
    ModalAction actions[] = {{"Close", ButtonStylePrimary, 0}};
    SetUIClipboardTextValue("");
    InjectReset();
    for(int frame = 0; frame < 5; frame++) {
        InjectMousePosition(frame < 2 ? 10 : 300,14);
        if(frame == 1) InjectMouseButton(MOUSE_BUTTON_LEFT,1);
        if(frame == 3) InjectMouseButton(MOUSE_BUTTON_LEFT,0);
        if(frame == 4) { InjectKey(KEY_LEFT_CONTROL,1); InjectKey(KEY_C,1); }
        InjectPump();
        BeginDrawing();
        ClearBackground(BLACK);
        BeginUIFrame(640,480,1);
        BeginTree(2);
        Text((TextProps){.bounds={10, 10, 0, 0}, .text="Select and copy this text.", .font=16, .color=WHITE, .wrap=TextWrapNone});
        InvalidateTree(UI_INVALIDATE_PAINT);
        EndTree();
        EndUIFrame();
        EndDrawing();
    }
    const char *copied = GetUIClipboardTextValue();
    if(copied == NULL || strcmp(copied,"Select and copy this text.") != 0) {
        fprintf(stderr,"retained text copy: %s\n",copied != NULL ? copied : "(null)");
        failures++;
    }
    InjectReset();
    Tab tabs[] = {{.label="One"},{.label="Two"},{.label="Three"},{.label="Four"}};
    int tab_scroll = 0;
    for(int frame = 0; frame < 2; frame++) {
        InjectMousePosition(230,115);
        InjectMouseButton(MOUSE_BUTTON_LEFT,frame == 0);
        InjectPump();
        BeginDrawing();
        BeginTextureMode(target);
        ClearBackground(RED);
        BeginUIFrame(640,480,1);
        int clicked = TabBar((TabBarProps){.bounds={10,100,180,32},.tabs=tabs,.count=4,.selected_index=0,.scroll_offset=&tab_scroll});
        EndUIFrame();
        EndTextureMode();
        EndDrawing();
        Image image = LoadImageFromTexture(target.texture);
        ImageFlipVertical(&image);
        Color outside = GetImageColor(image,230,115);
        if(clicked != -1 || outside.r != RED.r || outside.g != RED.g || outside.b != RED.b) {
            fprintf(stderr,"tab overflow frame %d: click=%d outside=%u,%u,%u\n",frame,clicked,outside.r,outside.g,outside.b);
            failures++;
        }
        UnloadImage(image);
    }
    InjectReset();
    for(int kind = 0; kind < 6; kind++) {
        for(int frame = 0; frame < 2; frame++) {
            BeginDrawing();
            BeginTextureMode(target);
            ClearBackground(BLACK);
            BeginUIFrame(640,480,1);
            BeginTree(1);
            Rect(10,10,20,20,RED,BLANK);
            Text((TextProps){.bounds={10, 45, 0, 0}, .text="Retained text", .font=16, .color=WHITE, .wrap=TextWrapNone});
            if(kind == 0) MessageDialog((MessageDialogProps){"Message","Hello","OK"});
            if(kind == 1) ConfirmDialog((ConfirmDialogProps){"Confirm","Continue?","No","Yes"});
            if(kind == 2) PromptDialog((PromptDialogProps){"Prompt",text,sizeof(text),&cursor,&focused,"Cancel","Save"});
            if(kind == 3) ActionModal((ModalProps){"Actions","Choose",actions,1,{0},300});
            if(kind == 4) PickerDialog((PickerDialogProps){"Picker",choices,NULL,2,"Cancel",300});
            if(kind == 5) ModalFrame(300,200,"Frame",(Texture2D){0},(Texture2D){0});
            Rect(40,10,20,20,GREEN,BLANK);
            EndTree();
            EndUIFrame();
            EndTextureMode();
            EndDrawing();
            Image image = LoadImageFromTexture(target.texture);
            ImageFlipVertical(&image);
            Color below = GetImageColor(image,15,15);
            Color above = GetImageColor(image,45,15);
            if(below.r >= 200 || above.g != GREEN.g || above.r != GREEN.r) {
                fprintf(stderr,"overlay %d frame %d: below=%u,%u,%u above=%u,%u,%u\n",kind,frame,below.r,below.g,below.b,above.r,above.g,above.b);
                failures++;
            }
            UnloadImage(image);
        }
    }
    /* The trigger's release is not an outside click dismissing its popover. */
    Rectangle anchor = {240,100,160,32};
    focused = 0;
    InjectReset();
    for(int frame = 0; frame < 3; frame++) {
        InjectMousePosition(frame < 2 ? 260 : 20, frame < 2 ? 110 : 20);
        InjectMouseButton(MOUSE_BUTTON_LEFT, frame != 1);
        InjectPump();
        BeginDrawing();
        BeginUIFrame(640,480,1);
        if(frame == 1) UIConsumePointerRelease();
        int result = TextPopover((TextPopoverProps){anchor,"Edit",text,sizeof(text),&cursor,&focused,972,300,31});
        EndUIFrame();
        EndDrawing();
        if((result != 0) != (frame == 2)) {
            fprintf(stderr,"popover frame %d returned %d\n",frame,result);
            failures++;
        }
    }
    InjectReset();
    UnloadRenderTexture(target);
    CloseWindow();
    return failures ? 1 : 0;
}
