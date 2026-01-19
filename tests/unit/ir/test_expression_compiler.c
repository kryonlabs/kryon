// ============================================================================
// EXPRESSION COMPILER TEST
// ============================================================================
// Test the expression compiler's ability to compile AST to bytecode

#define _POSIX_C_SOURCE 200809L
#include "ir_expression_compiler.h"
#include "ir_expression.h"
#include "ir_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("Test %d: %s...", tests_run, name); \
        fflush(stdout); \
    } while(0)

#define PASS() \
    do { \
        tests_passed++; \
        printf(" PASS\n"); \
    } while(0)

#define FAIL(msg) \
    do { \
        tests_failed++; \
        printf(" FAIL: %s\n", msg); \
    } while(0)

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            FAIL(msg); \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            FAIL(msg); \
            return; \
        } \
    } while(0)

// ============================================================================
// TESTS
// ============================================================================

static void test_compile_literal_int(void) {
    TEST("compile_literal_int");

    IRExpression* expr = ir_expr_int(42);
    ASSERT_TRUE(expr != NULL, "expression should not be null");

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code_size >= 2, "should have at least 2 instructions (PUSH_INT, HALT)");
    ASSERT_TRUE(compiled->code[0].opcode == OP_PUSH_INT, "first opcode should be PUSH_INT");
    ASSERT_TRUE(compiled->code[0].operand3 == 42, "value should be 42");
    ASSERT_TRUE(compiled->code[compiled->code_size - 1].opcode == OP_HALT, "last opcode should be HALT");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_literal_float(void) {
    TEST("compile_literal_float");

    IRExpression* expr = ir_expr_float(3.14);
    ASSERT_TRUE(expr != NULL, "expression should not be null");

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[0].opcode == OP_PUSH_FLOAT, "floats use OP_PUSH_FLOAT");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_literal_string(void) {
    TEST("compile_literal_string");

    IRExpression* expr = ir_expr_string("hello");
    ASSERT_TRUE(expr != NULL, "expression should not be null");

    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[0].opcode == OP_PUSH_STRING, "first opcode should be PUSH_STRING");
    ASSERT_TRUE(compiled->string_pool_count > 0, "should have string pool entry");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_literal_bool(void) {
    TEST("compile_literal_bool");

    IRExpression* expr1 = ir_expr_bool(1);
    IRCompiledExpr* compiled1 = ir_expr_compile(expr1);
    ASSERT_TRUE(compiled1 != NULL, "compiled true should not be null");
    ASSERT_TRUE(compiled1->code[0].opcode == OP_PUSH_BOOL, "first opcode should be PUSH_BOOL");
    ASSERT_TRUE(compiled1->code[0].operand1 == 1, "value should be 1 (true)");

    IRExpression* expr2 = ir_expr_bool(0);
    IRCompiledExpr* compiled2 = ir_expr_compile(expr2);
    ASSERT_TRUE(compiled2 != NULL, "compiled false should not be null");
    ASSERT_TRUE(compiled2->code[0].operand1 == 0, "value should be 0 (false)");

    ir_expr_free(expr1);
    ir_expr_compiled_free(compiled1);
    ir_expr_free(expr2);
    ir_expr_compiled_free(compiled2);
    PASS();
}

static void test_compile_literal_null(void) {
    TEST("compile_literal_null");

    IRExpression* expr = ir_expr_null();
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[0].opcode == OP_PUSH_NULL, "first opcode should be PUSH_NULL");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_var_ref(void) {
    TEST("compile_var_ref");

    IRExpression* expr = ir_expr_var("count");
    IRCompiledExpr* compiled = ir_expr_compile(expr);
    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[0].opcode == OP_LOAD_VAR, "first opcode should be LOAD_VAR");
    ASSERT_TRUE(compiled->string_pool_count > 0, "should have string pool entry");

    // Check that the variable name is in the pool
    uint16_t idx = compiled->code[0].operand2;
    ASSERT_TRUE(idx < compiled->string_pool_count, "pool index should be valid");
    ASSERT_TRUE(strcmp(compiled->string_pool[idx], "count") == 0, "pool entry should be 'count'");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_member_access(void) {
    TEST("compile_member_access");

    IRExpression* var_expr = ir_expr_var("user");
    IRExpression* member_expr = ir_expr_member_access(var_expr, "name");
    IRCompiledExpr* compiled = ir_expr_compile(member_expr);

    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code_size >= 3, "should have LOAD_VAR, GET_PROP, HALT");
    ASSERT_TRUE(compiled->code[0].opcode == OP_LOAD_VAR, "first opcode should be LOAD_VAR");
    ASSERT_TRUE(compiled->code[1].opcode == OP_GET_PROP, "second opcode should be GET_PROP");

    ir_expr_free(member_expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_member_access_nested(void) {
    TEST("compile_member_access_nested");

    // user.profile.name
    IRExpression* var_expr = ir_expr_var("user");
    IRExpression* profile_expr = ir_expr_member_access(var_expr, "profile");
    IRExpression* name_expr = ir_expr_member_access(profile_expr, "name");
    IRCompiledExpr* compiled = ir_expr_compile(name_expr);

    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[0].opcode == OP_LOAD_VAR, "first should be LOAD_VAR");
    ASSERT_TRUE(compiled->code[1].opcode == OP_GET_PROP, "second should be GET_PROP");
    ASSERT_TRUE(compiled->code[2].opcode == OP_GET_PROP, "third should be GET_PROP");

    ir_expr_free(name_expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_computed_member(void) {
    TEST("compile_computed_member");

    IRExpression* arr_expr = ir_expr_var("items");
    IRExpression* index_expr = ir_expr_int(0);
    IRExpression* computed_expr = ir_expr_computed_member(arr_expr, index_expr);
    IRCompiledExpr* compiled = ir_expr_compile(computed_expr);

    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[0].opcode == OP_LOAD_VAR, "first should be LOAD_VAR");
    ASSERT_TRUE(compiled->code[1].opcode == OP_PUSH_INT, "second should be PUSH_INT");
    ASSERT_TRUE(compiled->code[2].opcode == OP_GET_PROP_COMPUTED, "third should be GET_PROP_COMPUTED");

    ir_expr_free(computed_expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_binary_add(void) {
    TEST("compile_binary_add");

    IRExpression* expr = ir_expr_add(ir_expr_var("a"), ir_expr_var("b"));
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[0].opcode == OP_LOAD_VAR, "first should be LOAD_VAR");
    ASSERT_TRUE(compiled->code[1].opcode == OP_LOAD_VAR, "second should be LOAD_VAR");
    ASSERT_TRUE(compiled->code[2].opcode == OP_ADD, "third should be ADD");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_binary_comparison(void) {
    TEST("compile_binary_comparison");

    IRExpression* expr = ir_expr_binary(
        BINARY_OP_GT,
        ir_expr_var("count"),
        ir_expr_int(5)
    );
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[1].opcode == OP_PUSH_INT, "second should be PUSH_INT");
    ASSERT_TRUE(compiled->code[2].opcode == OP_GT, "third should be GT");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_logical_and(void) {
    TEST("compile_logical_and");

    IRExpression* expr = ir_expr_and(ir_expr_var("a"), ir_expr_var("b"));
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[2].opcode == OP_AND, "third should be AND");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_unary_not(void) {
    TEST("compile_unary_not");

    IRExpression* expr = ir_expr_unary(UNARY_OP_NOT, ir_expr_var("flag"));
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[0].opcode == OP_LOAD_VAR, "first should be LOAD_VAR");
    ASSERT_TRUE(compiled->code[1].opcode == OP_NOT, "second should be NOT");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_unary_negate(void) {
    TEST("compile_unary_negate");

    IRExpression* expr = ir_expr_unary(UNARY_OP_NEG, ir_expr_var("value"));
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[0].opcode == OP_LOAD_VAR, "first should be LOAD_VAR");
    ASSERT_TRUE(compiled->code[1].opcode == OP_NEGATE, "second should be NEGATE");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_ternary(void) {
    TEST("compile_ternary");

    IRExpression* expr = ir_expr_ternary(
        ir_expr_var("isValid"),
        ir_expr_int(1),
        ir_expr_int(0)
    );
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[0].opcode == OP_LOAD_VAR, "first should be LOAD_VAR");
    ASSERT_TRUE(compiled->code[1].opcode == OP_JUMP_IF_FALSE, "second should be JUMP_IF_FALSE");
    ASSERT_TRUE(compiled->code[2].opcode == OP_PUSH_INT, "third should be PUSH_INT");
    ASSERT_TRUE(compiled->code[2].operand3 == 1, "value should be 1");
    ASSERT_TRUE(compiled->code[3].opcode == OP_JUMP, "fourth should be JUMP");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_method_call(void) {
    TEST("compile_method_call");

    IRExpression* receiver = ir_expr_var("array");
    IRExpression* args[] = { ir_expr_var("item") };
    IRExpression* expr = ir_expr_method_call(receiver, "push", args, 1);
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[0].opcode == OP_LOAD_VAR, "first should be LOAD_VAR (receiver)");
    ASSERT_TRUE(compiled->code[1].opcode == OP_LOAD_VAR, "second should be LOAD_VAR (arg)");
    ASSERT_TRUE(compiled->code[2].opcode == OP_CALL_METHOD, "third should be CALL_METHOD");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_function_call(void) {
    TEST("compile_function_call");

    IRExpression* args[] = { ir_expr_var("a"), ir_expr_var("b") };
    IRExpression* expr = ir_expr_call("max", args, 2);
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[0].opcode == OP_LOAD_VAR, "first should be LOAD_VAR");
    ASSERT_TRUE(compiled->code[1].opcode == OP_LOAD_VAR, "second should be LOAD_VAR");
    ASSERT_TRUE(compiled->code[2].opcode == OP_CALL_FUNCTION, "third should be CALL_FUNCTION");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_compile_index_access(void) {
    TEST("compile_index_access");

    IRExpression* arr_expr = ir_expr_var("items");
    IRExpression* index_expr = ir_expr_int(0);
    IRExpression* expr = ir_expr_index(arr_expr, index_expr);
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[0].opcode == OP_LOAD_VAR, "first should be LOAD_VAR");
    ASSERT_TRUE(compiled->code[1].opcode == OP_PUSH_INT, "second should be PUSH_INT");
    ASSERT_TRUE(compiled->code[2].opcode == OP_GET_INDEX, "third should be GET_INDEX");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_const_fold_int_addition(void) {
    TEST("const_fold_int_addition");

    // 2 + 3 should fold to 5
    IRExpression* expr = ir_expr_add(ir_expr_int(2), ir_expr_int(3));
    IRExpression* optimized = ir_expr_optimize(expr);

    ASSERT_TRUE(optimized != NULL, "optimized should not be null");
    ASSERT_TRUE(optimized->type == EXPR_LITERAL_INT, "should be literal int");
    ASSERT_TRUE(optimized->int_value == 5, "value should be 5");

    ir_expr_free(expr);
    ir_expr_free(optimized);
    PASS();
}

static void test_const_fold_string_concat(void) {
    TEST("const_fold_string_concat");

    // "hello" + " world" should fold to "hello world"
    IRExpression* expr = ir_expr_add(ir_expr_string("hello"), ir_expr_string(" world"));
    IRExpression* optimized = ir_expr_optimize(expr);

    ASSERT_TRUE(optimized != NULL, "optimized should not be null");
    ASSERT_TRUE(optimized->type == EXPR_LITERAL_STRING, "should be literal string");
    ASSERT_TRUE(strcmp(optimized->string_value, "hello world") == 0, "value should be 'hello world'");

    ir_expr_free(expr);
    ir_expr_free(optimized);
    PASS();
}

static void test_const_fold_comparison(void) {
    TEST("const_fold_comparison");

    // 5 > 3 should fold to true
    IRExpression* expr = ir_expr_binary(BINARY_OP_GT, ir_expr_int(5), ir_expr_int(3));
    IRExpression* optimized = ir_expr_optimize(expr);

    ASSERT_TRUE(optimized != NULL, "optimized should not be null");
    ASSERT_TRUE(optimized->type == EXPR_LITERAL_BOOL, "should be literal bool");
    ASSERT_TRUE(optimized->bool_value == 1, "value should be true");

    ir_expr_free(expr);
    ir_expr_free(optimized);
    PASS();
}

static void test_const_fold_unary_not(void) {
    TEST("const_fold_unary_not");

    // !true should fold to false
    IRExpression* expr = ir_expr_unary(UNARY_OP_NOT, ir_expr_bool(1));
    IRExpression* optimized = ir_expr_optimize(expr);

    ASSERT_TRUE(optimized != NULL, "optimized should not be null");
    ASSERT_TRUE(optimized->type == EXPR_LITERAL_BOOL, "should be literal bool");
    ASSERT_TRUE(optimized->bool_value == 0, "value should be false");

    ir_expr_free(expr);
    ir_expr_free(optimized);
    PASS();
}

static void test_const_fold_ternary_true(void) {
    TEST("const_fold_ternary_true");

    // true ? 1 : 0 should fold to 1
    IRExpression* expr = ir_expr_ternary(
        ir_expr_bool(1),
        ir_expr_int(1),
        ir_expr_int(0)
    );
    IRExpression* optimized = ir_expr_optimize(expr);

    ASSERT_TRUE(optimized != NULL, "optimized should not be null");
    ASSERT_TRUE(optimized->type == EXPR_LITERAL_INT, "should be literal int");
    ASSERT_TRUE(optimized->int_value == 1, "value should be 1");

    ir_expr_free(expr);
    ir_expr_free(optimized);
    PASS();
}

static void test_const_fold_ternary_false(void) {
    TEST("const_fold_ternary_false");

    // false ? 1 : 0 should fold to 0
    IRExpression* expr = ir_expr_ternary(
        ir_expr_bool(0),
        ir_expr_int(1),
        ir_expr_int(0)
    );
    IRExpression* optimized = ir_expr_optimize(expr);

    ASSERT_TRUE(optimized != NULL, "optimized should not be null");
    ASSERT_TRUE(optimized->type == EXPR_LITERAL_INT, "should be literal int");
    ASSERT_TRUE(optimized->int_value == 0, "value should be 0");

    ir_expr_free(expr);
    ir_expr_free(optimized);
    PASS();
}

static void test_complex_expression(void) {
    TEST("complex_expression");

    // count > 5 ? "many" : "few"
    IRExpression* expr = ir_expr_ternary(
        ir_expr_binary(BINARY_OP_GT, ir_expr_var("count"), ir_expr_int(5)),
        ir_expr_string("many"),
        ir_expr_string("few")
    );
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->max_stack_depth > 0, "should track stack depth");
    ASSERT_TRUE(compiled->string_pool_count > 0, "should have string pool entries");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_disassembly(void) {
    TEST("disassembly");

    IRExpression* expr = ir_expr_add(ir_expr_var("a"), ir_expr_var("b"));
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    char* disasm = ir_bytecode_disassemble(compiled);
    ASSERT_TRUE(disasm != NULL, "disassembly should not be null");
    ASSERT_TRUE(strstr(disasm, "LOAD_VAR") != NULL, "should contain LOAD_VAR");
    ASSERT_TRUE(strstr(disasm, "ADD") != NULL, "should contain ADD");

    free(disasm);
    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

static void test_large_integer(void) {
    TEST("large_integer");

    // Integer that doesn't fit in int32_t
    int64_t large_val = (int64_t)INT32_MAX + 100;
    IRExpression* expr = ir_expr_int(large_val);
    IRCompiledExpr* compiled = ir_expr_compile(expr);

    ASSERT_TRUE(compiled != NULL, "compiled should not be null");
    ASSERT_TRUE(compiled->code[0].opcode == OP_PUSH_INT, "first should be PUSH_INT");
    ASSERT_TRUE(compiled->int_pool_count > 0, "should use int pool for large value");

    ir_expr_free(expr);
    ir_expr_compiled_free(compiled);
    PASS();
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("============================================================================\n");
    printf("EXPRESSION COMPILER TEST\n");
    printf("============================================================================\n\n");

    // Literal compilation tests
    test_compile_literal_int();
    test_compile_literal_float();
    test_compile_literal_string();
    test_compile_literal_bool();
    test_compile_literal_null();

    // Variable reference compilation
    test_compile_var_ref();

    // Member access compilation
    test_compile_member_access();
    test_compile_member_access_nested();
    test_compile_computed_member();

    // Binary operation compilation
    test_compile_binary_add();
    test_compile_binary_comparison();
    test_compile_logical_and();

    // Unary operation compilation
    test_compile_unary_not();
    test_compile_unary_negate();

    // Ternary compilation
    test_compile_ternary();

    // Method/function call compilation
    test_compile_method_call();
    test_compile_function_call();

    // Index access compilation
    test_compile_index_access();

    // Constant folding tests
    test_const_fold_int_addition();
    test_const_fold_string_concat();
    test_const_fold_comparison();
    test_const_fold_unary_not();
    test_const_fold_ternary_true();
    test_const_fold_ternary_false();

    // Complex expression tests
    test_complex_expression();

    // Utility tests
    test_disassembly();
    test_large_integer();

    printf("\n============================================================================\n");
    printf("RESULTS: %d total, %d passed, %d failed\n", tests_run, tests_passed, tests_failed);
    printf("============================================================================\n");

    return tests_failed > 0 ? 1 : 0;
}
