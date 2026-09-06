#include "kryon.h"
#include "ui_scaling.h"
#include "theme.h"


int CarouselControls(CarouselControlsProps p)
{
    if (p.count <= 0) return -1;
    int selected = ((p.selected % p.count) + p.count) % p.count;
    if (p.count == 1) return selected;
    if (!p.disabled) selected = (selected + p.move % p.count + p.count) % p.count;
    int hit = ScaleUIPx(48), inset = ScaleUIPx(12);
    int icon = ScaleUIPx(10);
    if (p.bounds.width >= hit*2 && p.bounds.height >= hit) {
        for (int i=0;i<2;i++) {
            Rectangle b = {i ? p.bounds.x+p.bounds.width-inset-hit : p.bounds.x+inset,
                           p.bounds.y+(p.bounds.height-hit)/2,hit,hit};
            Vector2 center = {b.x+hit/2,b.y+hit/2};
            Color fg = Fade(WHITE,p.disabled ? 0.35f : 1.0f);
            int d = i ? 1 : -1;
            Vector2 tip = {center.x+d*icon/2,center.y};
            if (IsWindowReady()) {
            DrawCircleV(center,hit/2,Fade(BLACK,0.38f));
            DrawLineEx((Vector2){center.x-d*icon/2,center.y-icon},tip,ScaleUIPx(2),fg);
            DrawLineEx(tip,(Vector2){center.x-d*icon/2,center.y+icon},ScaleUIPx(2),fg);
            }
            if (InvisibleButton((InvisibleButtonProps){.bounds=b,.id=p.id+i,.disabled=p.disabled}))
                selected = (selected+d+p.count)%p.count;
        }
    }
    if (p.indicators.width >= hit*p.count && p.indicators.height >= hit) {
        float x = p.indicators.x+(p.indicators.width-hit*p.count)/2;
        float y = p.indicators.y+(p.indicators.height-hit)/2;
        for (int i=0;i<p.count;i++) {
            Rectangle b = {x+i*hit,y,hit,hit};
            if (InvisibleButton((InvisibleButtonProps){.bounds=b,.id=p.id+2+i,.disabled=p.disabled})) selected=i;
            if (IsWindowReady()) DrawCircle((int)b.x+hit/2,(int)b.y+hit/2,ScaleUIPx(i==selected?4:3),
                       Fade(GetThemeText(),p.disabled?0.2f:(i==selected?1.0f:0.28f)));
        }
    }
    return selected;
}
