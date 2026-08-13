#include "krb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
check(int cond, const char *msg)
{
    if(cond)
        return 1;
    fprintf(stderr, "krb_walk_test: %s\n", msg);
    return 0;
}

int
main(int argc, char **argv)
{
    KrbImage img;
    unsigned nodes;
    unsigned imports;
    KrbNode node;
    unsigned i;
    int saw_bg = 0;
    int saw_text = 0;
    int saw_button = 0;

    if(argc < 2) {
        fprintf(stderr, "usage: krb_walk_test file.krb\n");
        return 2;
    }
    memset(&img, 0, sizeof(img));
    KryBackendSelect(&KryBackendNull);
    if(KrbLoadFile(&img, argv[1]) != 0) {
        fprintf(stderr, "krb_walk_test: load failed: %s\n", argv[1]);
        return 1;
    }
    nodes = KrbNodeCount(&img);
    imports = KrbImportCount(&img);
    if(!check(nodes > 0, "expected at least one node"))
        return 1;
    for(i = 0; i < nodes; i++) {
        if(KrbReadNode(&img, i, &node) != 0)
            return 1;
        if(node.type == KRB_NODE_BACKGROUND)
            saw_bg = 1;
        if(node.type == KRB_NODE_TEXT)
            saw_text = 1;
        if(node.type == KRB_NODE_BUTTON)
            saw_button = 1;
    }
    if(!check(saw_bg, "missing BACKGROUND node"))
        return 1;
    if(!check(saw_text, "missing TEXT node"))
        return 1;
    if(!check(saw_button, "missing BUTTON node"))
        return 1;
    if(!check(imports >= 3, "expected three button imports"))
        return 1;
    if(!check(KrbBind(&img, KrbImportName(&img, 0), NULL, NULL) == 0,
              "bind first import"))
        return 1;
    KrbDraw(&img, 0, 0, 800, 600);
    KrbFree(&img);
    printf("ok nodes=%u imports=%u\n", nodes, imports);
    return 0;
}
