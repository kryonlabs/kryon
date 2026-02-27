#include <stdio.h>
#include "src/kryon/parser.h"

int main() {
    KryonNode *ast = kryon_parse_file("examples/widgets.kry");
    if (ast == NULL) {
        printf("Parse failed!\n");
        return 1;
    }
    
    printf("Parse succeeded! Root has %d children\n", ast->nchildren);
    
    int result = kryon_execute_ast(ast);
    printf("Execute result: %d windows created\n", result);
    
    kryon_free_ast(ast);
    return 0;
}
