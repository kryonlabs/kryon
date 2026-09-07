#include "kryon.h"
#include "ui_scaling.h"
#include "theme.h"


int CarouselControls(CarouselControlsProps p)
{
    if(p.count <= 0)
        return -1;

    int selected = ((p.selected % p.count) + p.count) % p.count;
    if(p.count == 1)
        return selected;
    if(!p.disabled)
        selected = (selected + p.move % p.count + p.count) % p.count;

    int hit = Scale(56);
    int inset = Scale(12);
    int icon = Scale(14);
    if(p.bounds.width >= hit * 2 && p.bounds.height >= hit) {
        for(int i = 0; i < 2; i++) {
            Rectangle bounds = {
                i ? p.bounds.x + p.bounds.width - inset - hit
                  : p.bounds.x + inset,
                p.bounds.y + (p.bounds.height - hit) / 2,
                hit,
                hit
            };
            Vector2 center = {bounds.x + hit / 2, bounds.y + hit / 2};
            Color foreground = Fade(WHITE, p.disabled ? 0.35f : 1.0f);
            int d = i ? 1 : -1;
            Vector2 tip = {center.x + d * icon / 2, center.y};

            if(IsWindowReady()) {
                DrawCircleV(center, hit / 2, Fade(BLACK, 0.46f));
                DrawLineEx((Vector2){center.x - d * icon / 2,
                                     center.y - icon},
                           tip, Scale(3), foreground);
                DrawLineEx(tip,
                           (Vector2){center.x - d * icon / 2,
                                     center.y + icon},
                           Scale(3), foreground);
            }
            if(InvisibleButton((InvisibleButtonProps){
                   .bounds = bounds,
                   .id = p.id + i,
                   .disabled = p.disabled
               })) {
                selected = (selected + d + p.count) % p.count;
            }
        }
    }

    int indicator_hit = Scale(32);
    if(p.indicators.width >= indicator_hit * p.count &&
       p.indicators.height >= indicator_hit) {
        float x = p.indicators.x +
                  (p.indicators.width - indicator_hit * p.count) / 2;
        float y = p.indicators.y +
                  (p.indicators.height - indicator_hit) / 2;
        for(int i = 0; i < p.count; i++) {
            Rectangle bounds = {x + i * indicator_hit, y,
                                indicator_hit, indicator_hit};
            if(InvisibleButton((InvisibleButtonProps){
                   .bounds = bounds,
                   .id = p.id + 2 + i,
                   .disabled = p.disabled
               })) {
                selected = i;
            }
            if(IsWindowReady()) {
                DrawCircle((int)bounds.x + indicator_hit / 2,
                           (int)bounds.y + indicator_hit / 2,
                           Scale(i == selected ? 4 : 3),
                           Fade(GetThemeText(),
                                p.disabled ? 0.2f
                                           : (i == selected ? 1.0f : 0.28f)));
            }
        }
    }
    return selected;
}
