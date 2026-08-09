#include "kryon.h"
#include "theme.h"
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
    LabelTextFieldProps field = {.field_h = 40};
    SectionLabelProps section = {0};
    CheckboxRowProps checkbox = {0};
    ButtonRowProps row = {.width = 240, .height = 40};
    BottomNavProps nav = {0};
    TabBarProps tabs = {0};
    const UIWidgetNode *nodes;
    const UIWidgetNode *node;
    UINodeId group;
    UINodeId nested;
    int count = 0;

    SetThemeStyle(THEME_STYLE_RETRO);

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
    SetThemeStyle(THEME_STYLE_MATERIAL);
    check_int("material bottom nav",
              UIGetNodeHeight(UINodeBottomNav(nav)),
              ScaleUIPx(64));
    SetThemeStyle(THEME_STYLE_RETRO);
    check_int("retro tab bar",
              UIGetNodeHeight(UINodeTabBar(tabs)),
              ScaleUIPx(36));
    SetThemeStyle(THEME_STYLE_MATERIAL);
    check_int("material tab bar",
              UIGetNodeHeight(UINodeTabBar(tabs)),
              ScaleUIPx(48));
    check_int("title bar custom",
              UIGetNodeHeight(UINodeTitleBar(64)),
              64);

    UIBeginTree(7);
    group = BeginNodeGroup(11, (Rectangle){10, 10, 100, 80});
    nested = BeginNodeGroup(12, (Rectangle){20, 20, 40, 30});
    EndNodeGroup();
    EndNodeGroup();
    UIEndTree();

    nodes = UIGetTreeNodes(&count);
    check_int("tree count", count, 3);
    check_int("root parent", nodes[0].parent, -1);
    check_int("root first child", nodes[0].first_child, group);
    check_int("group parent", nodes[group].parent, 0);
    check_int("group first child", nodes[group].first_child, nested);
    check_int("nested parent", nodes[nested].parent, group);
    check_int("hit nested", UIHitTestNode((Vector2){25, 25}), nested);
    node = UIGetNode(group);
    check_int("get group", node != NULL ? node->id : -1, 11);

    return failures == 0 ? 0 : 1;
}
