#include "kryon.h"
#include <stdio.h>

static int failures;

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "FAIL: %s got %d want %d\n", name, got, want);
    failures++;
}

int
main(void)
{
    UILabelTextField field = {.field_h = 40};
    UISectionLabel section = {0};
    UICheckboxRow checkbox = {0};
    UIButtonRow row = {.width = 240, .height = 40};
    UIBottomNav nav = {0};
    UITabBar tabs = {0};

    check_int("section label",
              UIGetNodeHeight(UINodeSectionLabel(section, 0, 0)),
              ScaleUIPx(24));
    check_int("checkbox row",
              UIGetNodeHeight(UINodeCheckboxRow(checkbox, 0, 0)),
              ScaleUIPx(42));
    check_int("label text field",
              UIGetNodeHeight(UINodeLabelTextField(field, 0, 0, 240)),
              ScaleUIPx(22) + ScaleUIPx(40) + ScaleUIPx(24));
    check_int("button row",
              UIGetNodeHeight(UINodeButtonRow(row)),
              ScaleUIPx(40));
    check_int("bottom nav",
              UIGetNodeHeight(UINodeBottomNav(nav)),
              ScaleUIPx(40));
    check_int("tab bar",
              UIGetNodeHeight(UINodeTabBar(tabs)),
              ScaleUIPx(36));
    check_int("title bar custom",
              UIGetNodeHeight(UINodeTitleBar(64)),
              64);

    return failures == 0 ? 0 : 1;
}
