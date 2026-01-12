// ============================================================================
// EXPRESSION PARSER TEST
// ============================================================================
// Test the expression parser's ability to parse various expression types

#define _POSIX_C_SOURCE 200809L
#include "ir_expression_parser.h"
#include "ir_expression.h"
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

// ============================================================================
// TESTS
// ============================================================================

static void test_literal_int(void) {
    TEST("literal_int");

    IRExpression* expr = ir_parse_expression("42");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_LITERAL_INT, "should be literal int");
    ASSERT_TRUE(expr->int_value == 42, "value should be 42");

    ir_expr_free(expr);
    PASS();
}

static void test_literal_float(void) {
    TEST("literal_float");

    IRExpression* expr = ir_parse_expression("3.14");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_LITERAL_FLOAT, "should be literal float");
    ASSERT_TRUE(expr->float_value > 3.13 && expr->float_value < 3.15, "value should be 3.14");

    ir_expr_free(expr);
    PASS();
}

static void test_literal_string(void) {
    TEST("literal_string");

    IRExpression* expr = ir_parse_expression("\"hello\"");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_LITERAL_STRING, "should be literal string");
    ASSERT_TRUE(strcmp(expr->string_value, "hello") == 0, "value should be 'hello'");

    ir_expr_free(expr);
    PASS();
}

static void test_literal_bool(void) {
    TEST("literal_bool");

    IRExpression* expr1 = ir_parse_expression("true");
    ASSERT_TRUE(expr1 != NULL, "true expression should not be null");
    ASSERT_TRUE(expr1->type == EXPR_LITERAL_BOOL, "should be literal bool");
    ASSERT_TRUE(expr1->bool_value == 1, "value should be true");

    IRExpression* expr2 = ir_parse_expression("false");
    ASSERT_TRUE(expr2 != NULL, "false expression should not be null");
    ASSERT_TRUE(expr2->type == EXPR_LITERAL_BOOL, "should be literal bool");
    ASSERT_TRUE(expr2->bool_value == 0, "value should be false");

    ir_expr_free(expr1);
    ir_expr_free(expr2);
    PASS();
}

static void test_literal_null(void) {
    TEST("literal_null");

    IRExpression* expr = ir_parse_expression("null");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_LITERAL_NULL, "should be literal null");

    ir_expr_free(expr);
    PASS();
}

static void test_var_ref(void) {
    TEST("var_ref");

    IRExpression* expr = ir_parse_expression("count");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_VAR_REF, "should be var ref");
    ASSERT_TRUE(strcmp(expr->var_ref.name, "count") == 0, "name should be 'count'");

    ir_expr_free(expr);
    PASS();
}

static void test_member_access_simple(void) {
    TEST("member_access_simple");

    IRExpression* expr = ir_parse_expression("user.name");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_MEMBER_ACCESS, "should be member access");
    ASSERT_TRUE(strcmp(expr->member_access.property, "name") == 0, "property should be 'name'");
    ASSERT_TRUE(expr->member_access.object->type == EXPR_VAR_REF, "object should be var ref");
    ASSERT_TRUE(strcmp(expr->member_access.object->var_ref.name, "user") == 0, "object name should be 'user'");

    ir_expr_free(expr);
    PASS();
}

static void test_member_access_nested(void) {
    TEST("member_access_nested");

    IRExpression* expr = ir_parse_expression("user.profile.name");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_MEMBER_ACCESS, "should be member access");
    ASSERT_TRUE(strcmp(expr->member_access.property, "name") == 0, "property should be 'name'");

    // Check nested structure
    ASSERT_TRUE(expr->member_access.object->type == EXPR_MEMBER_ACCESS, "object should be member access");
    ASSERT_TRUE(strcmp(expr->member_access.object->member_access.property, "profile") == 0, "nested property should be 'profile'");

    ir_expr_free(expr);
    PASS();
}

static void test_computed_member_simple(void) {
    TEST("computed_member_simple");

    IRExpression* expr = ir_parse_expression("items[index]");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_COMPUTED_MEMBER, "should be computed member");
    ASSERT_TRUE(expr->computed_member.object->type == EXPR_VAR_REF, "object should be var ref");
    ASSERT_TRUE(expr->computed_member.key->type == EXPR_VAR_REF, "key should be var ref");

    ir_expr_free(expr);
    PASS();
}

static void test_computed_member_literal_index(void) {
    TEST("computed_member_literal_index");

    IRExpression* expr = ir_parse_expression("items[0]");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_COMPUTED_MEMBER, "should be computed member");
    ASSERT_TRUE(expr->computed_member.key->type == EXPR_LITERAL_INT, "key should be literal int");
    ASSERT_TRUE(expr->computed_member.key->int_value == 0, "key value should be 0");

    ir_expr_free(expr);
    PASS();
}

static void test_method_call_no_args(void) {
    TEST("method_call_no_args");

    IRExpression* expr = ir_parse_expression("array.clear()");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_METHOD_CALL, "should be method call");
    ASSERT_TRUE(strcmp(expr->method_call.method_name, "clear") == 0, "method should be 'clear'");
    ASSERT_TRUE(expr->method_call.arg_count == 0, "should have 0 args");

    ir_expr_free(expr);
    PASS();
}

static void test_method_call_with_args(void) {
    TEST("method_call_with_args");

    IRExpression* expr = ir_parse_expression("array.push(item)");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_METHOD_CALL, "should be method call");
    ASSERT_TRUE(strcmp(expr->method_call.method_name, "push") == 0, "method should be 'push'");
    ASSERT_TRUE(expr->method_call.arg_count == 1, "should have 1 arg");
    ASSERT_TRUE(expr->method_call.args[0]->type == EXPR_VAR_REF, "arg should be var ref");
    ASSERT_TRUE(strcmp(expr->method_call.args[0]->var_ref.name, "item") == 0, "arg name should be 'item'");

    ir_expr_free(expr);
    PASS();
}

static void test_method_call_multiple_args(void) {
    TEST("method_call_multiple_args");

    IRExpression* expr = ir_parse_expression("list.insert(0, item)");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_METHOD_CALL, "should be method call");
    ASSERT_TRUE(strcmp(expr->method_call.method_name, "insert") == 0, "method should be 'insert'");
    ASSERT_TRUE(expr->method_call.arg_count == 2, "should have 2 args");

    ir_expr_free(expr);
    PASS();
}

static void test_function_call(void) {
    TEST("function_call");

    IRExpression* expr = ir_parse_expression("toString()");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_CALL, "should be function call");
    ASSERT_TRUE(strcmp(expr->call.function, "toString") == 0, "function should be 'toString'");
    ASSERT_TRUE(expr->call.arg_count == 0, "should have 0 args");

    ir_expr_free(expr);
    PASS();
}

static void test_function_call_with_args(void) {
    TEST("function_call_with_args");

    IRExpression* expr = ir_parse_expression("max(a, b)");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_CALL, "should be function call");
    ASSERT_TRUE(strcmp(expr->call.function, "max") == 0, "function should be 'max'");
    ASSERT_TRUE(expr->call.arg_count == 2, "should have 2 args");

    ir_expr_free(expr);
    PASS();
}

static void test_binary_add(void) {
    TEST("binary_add");

    IRExpression* expr = ir_parse_expression("a + b");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_BINARY, "should be binary");
    ASSERT_TRUE(expr->binary.op == BINARY_OP_ADD, "op should be ADD");
    ASSERT_TRUE(expr->binary.left->type == EXPR_VAR_REF, "left should be var ref");
    ASSERT_TRUE(expr->binary.right->type == EXPR_VAR_REF, "right should be var ref");

    ir_expr_free(expr);
    PASS();
}

static void test_binary_comparison(void) {
    TEST("binary_comparison");

    IRExpression* expr = ir_parse_expression("count > 5");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_BINARY, "should be binary");
    ASSERT_TRUE(expr->binary.op == BINARY_OP_GT, "op should be GT");
    ASSERT_TRUE(expr->binary.right->type == EXPR_LITERAL_INT, "right should be literal int");
    ASSERT_TRUE(expr->binary.right->int_value == 5, "right value should be 5");

    ir_expr_free(expr);
    PASS();
}

static void test_logical_and(void) {
    TEST("logical_and");

    IRExpression* expr = ir_parse_expression("a && b");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_BINARY, "should be binary");
    ASSERT_TRUE(expr->binary.op == BINARY_OP_AND, "op should be AND");

    ir_expr_free(expr);
    PASS();
}

static void test_logical_or(void) {
    TEST("logical_or");

    IRExpression* expr = ir_parse_expression("a || b");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_BINARY, "should be binary");
    ASSERT_TRUE(expr->binary.op == BINARY_OP_OR, "op should be OR");

    ir_expr_free(expr);
    PASS();
}

static void test_unary_not(void) {
    TEST("unary_not");

    IRExpression* expr = ir_parse_expression("!flag");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_UNARY, "should be unary");
    ASSERT_TRUE(expr->unary.op == UNARY_OP_NOT, "op should be NOT");

    ir_expr_free(expr);
    PASS();
}

static void test_unary_negate(void) {
    TEST("unary_negate");

    IRExpression* expr = ir_parse_expression("-value");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_UNARY, "should be unary");
    ASSERT_TRUE(expr->unary.op == UNARY_OP_NEG, "op should be NEG");

    ir_expr_free(expr);
    PASS();
}

static void test_ternary_simple(void) {
    TEST("ternary_simple");

    IRExpression* expr = ir_parse_expression("isValid ? 1 : 0");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_TERNARY, "should be ternary");
    ASSERT_TRUE(expr->ternary.then_expr->type == EXPR_LITERAL_INT, "then should be literal int");
    ASSERT_TRUE(expr->ternary.then_expr->int_value == 1, "then value should be 1");
    ASSERT_TRUE(expr->ternary.else_expr->type == EXPR_LITERAL_INT, "else should be literal int");
    ASSERT_TRUE(expr->ternary.else_expr->int_value == 0, "else value should be 0");

    ir_expr_free(expr);
    PASS();
}

static void test_group_simple(void) {
    TEST("group_simple");

    IRExpression* expr = ir_parse_expression("(a + b)");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_GROUP, "should be group");
    ASSERT_TRUE(expr->group.inner->type == EXPR_BINARY, "inner should be binary");

    ir_expr_free(expr);
    PASS();
}

static void test_complex_expression(void) {
    TEST("complex_expression");

    IRExpression* expr = ir_parse_expression("user.profile.name.length");
    ASSERT_TRUE(expr != NULL, "expression should not be null");
    ASSERT_TRUE(expr->type == EXPR_MEMBER_ACCESS, "should be member access");
    ASSERT_TRUE(strcmp(expr->member_access.property, "length") == 0, "property should be 'length'");

    ir_expr_free(expr);
    PASS();
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    printf("============================================================================\n");
    printf("EXPRESSION PARSER TEST\n");
    printf("============================================================================\n\n");

    // Literals
    test_literal_int();
    test_literal_float();
    test_literal_string();
    test_literal_bool();
    test_literal_null();

    // Variable reference
    test_var_ref();

    // Member access
    test_member_access_simple();
    test_member_access_nested();

    // Computed member
    test_computed_member_simple();
    test_computed_member_literal_index();

    // Method calls
    test_method_call_no_args();
    test_method_call_with_args();
    test_method_call_multiple_args();

    // Function calls
    test_function_call();
    test_function_call_with_args();

    // Binary operators
    test_binary_add();
    test_binary_comparison();

    // Logical operators
    test_logical_and();
    test_logical_or();

    // Unary operators
    test_unary_not();
    test_unary_negate();

    // Ternary
    test_ternary_simple();

    // Grouping
    test_group_simple();

    // Complex expressions
    test_complex_expression();

    printf("\n============================================================================\n");
    printf("RESULTS: %d total, %d passed, %d failed\n", tests_run, tests_passed, tests_failed);
    printf("============================================================================\n");

    return tests_failed > 0 ? 1 : 0;
}
