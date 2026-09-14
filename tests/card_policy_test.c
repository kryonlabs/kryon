#include <assert.h>
#include <math.h>
#include <string.h>

#include "runtime/card.h"

int
main(void)
{
    CardProps card;
    ButtonProps button;

    memset(&card, 0, sizeof(card));
    card.bounds = (Rectangle){10, 20, 100, 50};
    card.id = 77;
    card.clickable = 1;
    card.class_name = 42;
    card.disabled = 1;
    card.selected = 1;
    card.state = ButtonStateFocus;

    button = CardButtonProps(card);
    assert(button.id == 77);
    assert(button.class_name == 42);
    assert(button.disabled);
    assert(button.selected);
    assert(button.state == ButtonStateFocus);
    assert(button.size == ControlSizeLarge);
    assert(fabsf(button.bounds.x - 10.0f) < 0.001f);
    assert(fabsf(button.bounds.width - 100.0f) < 0.001f);

    card.clickable = 0;
    button = CardButtonProps(card);
    assert(button.id == 0);
    assert(button.disabled);
    assert(button.selected);

    card.disabled = 0;
    card.selected = 0;
    card.state = ButtonStateNormal;
    button = CardButtonProps(card);
    assert(!button.disabled);
    assert(!button.selected);
    assert(button.state == ButtonStateNormal);

    return 0;
}
