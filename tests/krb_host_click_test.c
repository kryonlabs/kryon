#include "02_buttons.krb.h"
#include "krb.h"

#include <stdio.h>
#include <string.h>

int
main(void)
{
    int clicks = -1;
    char action[128];

    KryBackendSelect(&KryBackendNull);
    if(ButtonsExample_krb_read_i32("click_count", &clicks) != 0 || clicks != 0) {
        fprintf(stderr, "initial click_count got %d\n", clicks);
        return 1;
    }
    if(ButtonsExample_krb_press("primary_button") != 1)
        return 1;
    if(ButtonsExample_krb_press("danger_button") != 1)
        return 1;
    if(ButtonsExample_krb_read_i32("click_count", &clicks) != 0 || clicks != 2) {
        fprintf(stderr, "after press click_count got %d\n", clicks);
        return 1;
    }
    if(ButtonsExample_krb_read_cstr("last_action", action, sizeof(action)) != 0 ||
       strcmp(action, "Danger Button clicked.") != 0) {
        fprintf(stderr, "last_action got '%s'\n", action);
        return 1;
    }
    ButtonsExample_krb_draw(0, 0, 800, 600);
    printf("ok clicks=%d\n", clicks);
    return 0;
}
