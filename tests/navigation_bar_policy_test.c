#include <assert.h>
#include <math.h>

#include "runtime/navigation_bar.h"

int
main(void)
{
    assert(NavigationBarFactsFor(77, ButtonStateNormal).kind ==
           StyleKindNavigationBar());
    assert(NavigationBarFactsFor(77, ButtonStateNormal).class_name == 77);
    assert(NavigationBarFactsFor(77, ButtonStateNormal).role == StyleAny());
    assert(NavigationBarFactsFor(77, ButtonStateHover).state ==
           ButtonStateHover);
    assert(NavigationBarRoleFactsFor(88, NavigationBarPanelRole(),
                                     ButtonStateNormal).kind ==
           StyleKindNavigationBar());
    assert(NavigationBarRoleFactsFor(88, NavigationBarPanelRole(),
                                     ButtonStateNormal).role ==
           NavigationBarPanelRole());
    assert(NavigationBarRoleFactsFor(88, NavigationBarRowRole(),
                                     ButtonStateNormal).role ==
           NavigationBarRowRole());
    assert(NavigationBarRoleFactsFor(88, NavigationBarActionRole(),
                                     ButtonStateNormal).role ==
           NavigationBarActionRole());
    assert(NavigationBarRoleFactsFor(88, NavigationBarRouteRole(),
                                     ButtonStateNormal).role ==
           NavigationBarRouteRole());
    assert(NavigationBarRoleFactsFor(88, NavigationBarRowRole(),
                                     ButtonStateDisabled).state ==
           ButtonStateDisabled);
    assert(NavigationBarItemFactsFor(99, ButtonToneNeutral,
                                     ButtonEmphasisSoft,
                                     ButtonStateNormal).kind ==
           StyleKindNavigationBarItem());
    assert(NavigationBarItemFactsFor(99, ButtonToneAccent,
                                     ButtonEmphasisFilled,
                                     ButtonStateSelected).tone ==
           ButtonToneAccent);
    assert(NavigationBarItemFactsFor(99, ButtonToneAccent,
                                     ButtonEmphasisFilled,
                                     ButtonStateSelected).emphasis ==
           ButtonEmphasisFilled);
    assert(NavigationBarItemFactsFor(99, ButtonToneNeutral,
                                     ButtonEmphasisSoft,
                                     ButtonStateNormal).class_name == 99);
    assert(NavigationBarDefaultHeight(1.0f) == 86);
    assert(NavigationBarDefaultHeight(2.0f) == 172);
    assert(NavigationBarDefaultHeightFor(true, 1.0f) == 40);
    assert(NavigationBarDefaultHeightFor(false, 1.0f) == 86);
    return 0;
}
