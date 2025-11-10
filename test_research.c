#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

void print_separator() {
    printf("\n========================================\n\n");
}

/* Test 1: AST Construction and Evaluation */
void test_ast_construction() {
    printf("=== TEST 1: AST Construction & Evaluation ===\n\n");

    /* Build expression: 2 * x + 3 */
    ASTNode *two = ast_create_number(2.0);
    ASTNode *x = ast_create_variable("x");
    ASTNode *mul = ast_create_binary_op(OP_MULTIPLY, two, x);
    ASTNode *three = ast_create_number(3.0);
    ASTNode *expr = ast_create_binary_op(OP_ADD, mul, three);

    printf("Expression: 2 * x + 3\n\n");
    ast_print(expr);

    /* Evaluate with x=5 */
    double value = 5.0;
    VarMapping mapping = {"x", 0};
    VarContext ctx = {
        .values = &value,
        .count = 1,
        .mappings = &mapping,
        .mapping_count = 1
    };

    double result = ast_evaluate(expr, &ctx);
    printf("\nWith x=%.0f: Result = %.2f (expected 13.00)\n", value, result);

    char *str = ast_to_string(expr);
    printf("String form: %s\n", str);
    free(str);

    ast_free(expr);
    print_separator();
}

/* Test 2: Symbolic Differentiation */
void test_differentiation() {
    printf("=== TEST 2: Symbolic Differentiation ===\n\n");

    /* d/dx(x^2) = 2*x */
    printf("1. d/dx(x^2) = 2*x\n");
    ASTNode *x1 = ast_create_variable("x");
    ASTNode *two1 = ast_create_number(2.0);
    ASTNode *x_squared = ast_create_binary_op(OP_POWER, x1, two1);

    ASTNode *derivative1 = ast_differentiate(x_squared, "x");
    ASTNode *simplified1 = ast_simplify(derivative1);

    char *d1_str = ast_to_string(simplified1);
    printf("   Result: %s\n\n", d1_str);
    free(d1_str);
    ast_free(x_squared);
    ast_free(simplified1);

    /* d/dx(3*x + 5) = 3 */
    printf("2. d/dx(3*x + 5) = 3\n");
    ASTNode *three = ast_create_number(3.0);
    ASTNode *x2 = ast_create_variable("x");
    ASTNode *three_x = ast_create_binary_op(OP_MULTIPLY, three, x2);
    ASTNode *five = ast_create_number(5.0);
    ASTNode *linear = ast_create_binary_op(OP_ADD, three_x, five);

    ASTNode *derivative2 = ast_differentiate(linear, "x");
    ASTNode *simplified2 = ast_simplify(derivative2);

    char *d2_str = ast_to_string(simplified2);
    printf("   Result: %s\n\n", d2_str);
    free(d2_str);
    ast_free(linear);
    ast_free(simplified2);

    /* d/dx(sin(x)) = cos(x) */
    printf("3. d/dx(sin(x)) = cos(x)\n");
    ASTNode *x3 = ast_create_variable("x");
    ASTNode *args1[] = {x3};
    ASTNode *sin_x = ast_create_function_call("SIN", args1, 1);

    ASTNode *derivative3 = ast_differentiate(sin_x, "x");
    char *d3_str = ast_to_string(derivative3);
    printf("   Result: %s\n\n", d3_str);
    free(d3_str);
    ast_free(sin_x);
    ast_free(derivative3);

    print_separator();
}

/* Test 3: Expression Simplification */
void test_simplification() {
    printf("=== TEST 3: Expression Simplification ===\n\n");

    /* x + 0 => x */
    printf("1. x + 0 => x\n");
    ASTNode *x1 = ast_create_variable("x");
    ASTNode *zero1 = ast_create_number(0.0);
    ASTNode *expr1 = ast_create_binary_op(OP_ADD, x1, zero1);

    char *before1 = ast_to_string(expr1);
    printf("   Before: %s\n", before1);
    free(before1);

    ASTNode *simplified1 = ast_simplify(expr1);
    char *after1 = ast_to_string(simplified1);
    printf("   After:  %s\n\n", after1);
    free(after1);
    ast_free(simplified1);

    /* x * 1 => x */
    printf("2. x * 1 => x\n");
    ASTNode *x2 = ast_create_variable("x");
    ASTNode *one = ast_create_number(1.0);
    ASTNode *expr2 = ast_create_binary_op(OP_MULTIPLY, x2, one);

    char *before2 = ast_to_string(expr2);
    printf("   Before: %s\n", before2);
    free(before2);

    ASTNode *simplified2 = ast_simplify(expr2);
    char *after2 = ast_to_string(simplified2);
    printf("   After:  %s\n\n", after2);
    free(after2);
    ast_free(simplified2);

    /* 2 + 3 => 5 (constant folding) */
    printf("3. 2 + 3 => 5 (constant folding)\n");
    ASTNode *two = ast_create_number(2.0);
    ASTNode *three = ast_create_number(3.0);
    ASTNode *expr3 = ast_create_binary_op(OP_ADD, two, three);

    char *before3 = ast_to_string(expr3);
    printf("   Before: %s\n", before3);
    free(before3);

    ASTNode *simplified3 = ast_simplify(expr3);
    char *after3 = ast_to_string(simplified3);
    printf("   After:  %s\n\n", after3);
    free(after3);
    ast_free(simplified3);

    /* -(-x) => x (double negation) */
    printf("4. -(-x) => x (double negation)\n");
    ASTNode *x4 = ast_create_variable("x");
    ASTNode *neg_x = ast_create_unary_op(OP_NEGATE, x4);
    ASTNode *expr4 = ast_create_unary_op(OP_NEGATE, neg_x);

    char *before4 = ast_to_string(expr4);
    printf("   Before: %s\n", before4);
    free(before4);

    ASTNode *simplified4 = ast_simplify(expr4);
    char *after4 = ast_to_string(simplified4);
    printf("   After:  %s\n\n", after4);
    free(after4);
    ast_free(simplified4);

    print_separator();
}

/* Test 4: Bytecode Compilation */
void test_bytecode_compilation() {
    printf("=== TEST 4: Bytecode Compilation ===\n\n");

    /* Build expression: 2 + 3 * 4 */
    ASTNode *two = ast_create_number(2.0);
    ASTNode *three = ast_create_number(3.0);
    ASTNode *four = ast_create_number(4.0);
    ASTNode *mul = ast_create_binary_op(OP_MULTIPLY, three, four);
    ASTNode *expr = ast_create_binary_op(OP_ADD, two, mul);

    printf("Expression: 2 + 3 * 4\n");
    ast_print(expr);
    printf("\n");

    /* Compile to bytecode */
    Bytecode *bc = ast_compile(expr);
    printf("Compiled bytecode:\n");
    bytecode_print(bc);

    /* Execute bytecode */
    VM *vm = vm_create(NULL);
    float result = vm_execute(vm, bc);
    printf("\nVM Execution Result: %.2f (expected 14.00)\n", result);

    vm_free(vm);
    bytecode_free(bc);
    ast_free(expr);
    print_separator();
}

/* Test 5: Complex Expression with Variables */
void test_complex_expression() {
    printf("=== TEST 5: Complex Expression with Variables ===\n\n");

    /* Build: sqrt(a^2 + b^2) - Pythagorean theorem */
    ASTNode *a1 = ast_create_variable("a");
    ASTNode *two1 = ast_create_number(2.0);
    ASTNode *a_squared = ast_create_binary_op(OP_POWER, a1, two1);

    ASTNode *b1 = ast_create_variable("b");
    ASTNode *two2 = ast_create_number(2.0);
    ASTNode *b_squared = ast_create_binary_op(OP_POWER, b1, two2);

    ASTNode *sum = ast_create_binary_op(OP_ADD, a_squared, b_squared);
    ASTNode *args[] = {sum};
    ASTNode *expr = ast_create_function_call("SQRT", args, 1);

    printf("Expression: sqrt(a^2 + b^2)\n");
    char *expr_str = ast_to_string(expr);
    printf("String form: %s\n\n", expr_str);
    free(expr_str);

    /* Evaluate with a=3, b=4 (should give 5) */
    double values[] = {3.0, 4.0};
    VarMapping mappings[] = {{"a", 0}, {"b", 1}};
    VarContext ctx = {
        .values = values,
        .count = 2,
        .mappings = mappings,
        .mapping_count = 2
    };

    double result = ast_evaluate(expr, &ctx);
    printf("With a=3, b=4:\n");
    printf("  Direct evaluation: %.2f (expected 5.00)\n", result);

    /* Also test via bytecode */
    Bytecode *bc = ast_compile(expr);
    VM *vm = vm_create(&ctx);
    float vm_result = vm_execute(vm, bc);
    printf("  Bytecode VM result: %.2f\n", vm_result);

    vm_free(vm);
    bytecode_free(bc);
    ast_free(expr);
    print_separator();
}

/* Test 6: AST Analysis Functions */
void test_ast_analysis() {
    printf("=== TEST 6: AST Analysis ===\n\n");

    /* Build: 2*x + 3*y + 5 */
    ASTNode *two = ast_create_number(2.0);
    ASTNode *x = ast_create_variable("x");
    ASTNode *two_x = ast_create_binary_op(OP_MULTIPLY, two, x);

    ASTNode *three = ast_create_number(3.0);
    ASTNode *y = ast_create_variable("y");
    ASTNode *three_y = ast_create_binary_op(OP_MULTIPLY, three, y);

    ASTNode *five = ast_create_number(5.0);
    ASTNode *sum1 = ast_create_binary_op(OP_ADD, two_x, three_y);
    ASTNode *expr = ast_create_binary_op(OP_ADD, sum1, five);

    char *expr_str = ast_to_string(expr);
    printf("Expression: %s\n\n", expr_str);
    free(expr_str);

    printf("Contains 'x': %s\n", ast_contains_variable(expr, "x") ? "YES" : "NO");
    printf("Contains 'y': %s\n", ast_contains_variable(expr, "y") ? "YES" : "NO");
    printf("Contains 'z': %s\n", ast_contains_variable(expr, "z") ? "YES" : "NO");
    printf("Operation count: %d\n", ast_count_operations(expr));

    ast_free(expr);
    print_separator();
}

/* Test 7: Differentiation with Chain Rule */
void test_chain_rule() {
    printf("=== TEST 7: Chain Rule Differentiation ===\n\n");

    /* d/dx(sqrt(x)) = 1/(2*sqrt(x)) */
    printf("1. d/dx(sqrt(x))\n");
    ASTNode *x1 = ast_create_variable("x");
    ASTNode *args1[] = {x1};
    ASTNode *sqrt_x = ast_create_function_call("SQRT", args1, 1);

    ASTNode *derivative1 = ast_differentiate(sqrt_x, "x");
    char *d1_str = ast_to_string(derivative1);
    printf("   Result: %s\n", d1_str);
    printf("   Expected: (1.00 / ((2.00) * SQRT(x)))\n\n");
    free(d1_str);
    ast_free(sqrt_x);
    ast_free(derivative1);

    /* d/dx(exp(x)) = exp(x) */
    printf("2. d/dx(exp(x))\n");
    ASTNode *x2 = ast_create_variable("x");
    ASTNode *args2[] = {x2};
    ASTNode *exp_x = ast_create_function_call("EXP", args2, 1);

    ASTNode *derivative2 = ast_differentiate(exp_x, "x");
    char *d2_str = ast_to_string(derivative2);
    printf("   Result: %s\n", d2_str);
    printf("   Expected: (EXP(x) * 1.00)\n\n");
    free(d2_str);
    ast_free(exp_x);
    ast_free(derivative2);

    /* d/dx(ln(x)) = 1/x */
    printf("3. d/dx(ln(x))\n");
    ASTNode *x3 = ast_create_variable("x");
    ASTNode *args3[] = {x3};
    ASTNode *ln_x = ast_create_function_call("LN", args3, 1);

    ASTNode *derivative3 = ast_differentiate(ln_x, "x");
    ASTNode *simplified3 = ast_simplify(derivative3);
    char *d3_str = ast_to_string(simplified3);
    printf("   Result: %s\n", d3_str);
    printf("   Expected: (1.00 / x)\n\n");
    free(d3_str);
    ast_free(ln_x);
    ast_free(simplified3);

    print_separator();
}

int main() {
    printf("\n");
    printf("==============================================\n");
    printf("  RESEARCH FEATURES TEST SUITE\n");
    printf("==============================================\n");
    print_separator();

    test_ast_construction();
    test_differentiation();
    test_simplification();
    test_bytecode_compilation();
    test_complex_expression();
    test_ast_analysis();
    test_chain_rule();

    printf("==============================================\n");
    printf("All research tests completed!\n");
    printf("==============================================\n\n");

    return 0;
}
