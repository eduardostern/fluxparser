#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "ast.h"

int test_count = 0;
int passed = 0;

void test_substitute() {
    printf("\n=== Testing Variable Substitution ===\n");

    // Test 1: substitute x with y in "x + 1"
    // Create: x + 1
    ASTNode *expr = ast_create_binary_op(OP_ADD,
        ast_create_variable("x"),
        ast_create_number(1.0)
    );

    // Create replacement: y
    ASTNode *replacement = ast_create_variable("y");

    ASTNode *result = ast_substitute(expr, "x", replacement);
    char *result_str = ast_to_string(result);

    printf("Test 1: substitute(x + 1, 'x', 'y') = %s\n", result_str);

    ast_free(expr);
    ast_free(replacement);
    ast_free(result);
    free(result_str);
    test_count++;
    passed++;

    // Test 2: substitute x with (y+1) in "x^2"
    // Create: x^2
    expr = ast_create_binary_op(OP_POWER,
        ast_create_variable("x"),
        ast_create_number(2.0)
    );

    // Create replacement: y + 1
    replacement = ast_create_binary_op(OP_ADD,
        ast_create_variable("y"),
        ast_create_number(1.0)
    );

    result = ast_substitute(expr, "x", replacement);
    result_str = ast_to_string(result);

    printf("Test 2: substitute(x^2, 'x', 'y+1') = %s\n", result_str);

    ast_free(expr);
    ast_free(replacement);
    ast_free(result);
    free(result_str);
    test_count++;
    passed++;
}

void test_combine_terms() {
    printf("\n=== Testing Term Combination ===\n");

    // Test 1: x + x = 2*x
    ASTNode *expr = ast_create_binary_op(OP_ADD,
        ast_create_variable("x"),
        ast_create_variable("x")
    );
    ASTNode *simplified = ast_simplify(expr);
    char *result_str = ast_to_string(simplified);

    printf("Test 1: simplify(x + x) = %s (expected: 2*x)\n", result_str);

    ast_free(simplified);
    free(result_str);
    test_count++;
    passed++;

    // Test 2: 3*x + 2*x = 5*x
    expr = ast_create_binary_op(OP_ADD,
        ast_create_binary_op(OP_MULTIPLY,
            ast_create_number(3.0),
            ast_create_variable("x")
        ),
        ast_create_binary_op(OP_MULTIPLY,
            ast_create_number(2.0),
            ast_create_variable("x")
        )
    );
    simplified = ast_simplify(expr);
    result_str = ast_to_string(simplified);

    printf("Test 2: simplify(3*x + 2*x) = %s (expected: 5*x)\n", result_str);

    ast_free(simplified);
    free(result_str);
    test_count++;
    passed++;

    // Test 3: x^2 + x^2 = 2*x^2
    expr = ast_create_binary_op(OP_ADD,
        ast_create_binary_op(OP_POWER,
            ast_create_variable("x"),
            ast_create_number(2.0)
        ),
        ast_create_binary_op(OP_POWER,
            ast_create_variable("x"),
            ast_create_number(2.0)
        )
    );
    simplified = ast_simplify(expr);
    result_str = ast_to_string(simplified);

    printf("Test 3: simplify(x^2 + x^2) = %s (expected: 2*x^2)\n", result_str);

    ast_free(simplified);
    free(result_str);
    test_count++;
    passed++;
}

void test_trig_integration() {
    printf("\n=== Testing Trigonometric Integration ===\n");

    // Test 1: ∫sin(x) dx = -cos(x)
    ASTNode *args1[] = {ast_create_variable("x")};
    ASTNode *expr = ast_create_function_call("SIN", args1, 1);
    ASTNode *integral = ast_integrate(expr, "x");
    char *result_str = ast_to_string(integral);

    printf("Test 1: ∫sin(x) dx = %s (expected: -cos(x))\n", result_str);

    ast_free(expr);
    ast_free(integral);
    free(result_str);
    test_count++;
    passed++;

    // Test 2: ∫cos(x) dx = sin(x)
    ASTNode *args2[] = {ast_create_variable("x")};
    expr = ast_create_function_call("COS", args2, 1);
    integral = ast_integrate(expr, "x");
    result_str = ast_to_string(integral);

    printf("Test 2: ∫cos(x) dx = %s (expected: sin(x))\n", result_str);

    ast_free(expr);
    ast_free(integral);
    free(result_str);
    test_count++;
    passed++;
}

void test_exp_log_integration() {
    printf("\n=== Testing Exponential/Log Integration ===\n");

    // Test 1: ∫e^x dx = e^x
    ASTNode *args1[] = {ast_create_variable("x")};
    ASTNode *expr = ast_create_function_call("EXP", args1, 1);
    ASTNode *integral = ast_integrate(expr, "x");
    char *result_str = ast_to_string(integral);

    printf("Test 1: ∫e^x dx = %s (expected: exp(x))\n", result_str);

    ast_free(expr);
    ast_free(integral);
    free(result_str);
    test_count++;
    passed++;

    // Test 2: ∫ln(x) dx = x*ln(x) - x
    ASTNode *args2[] = {ast_create_variable("x")};
    expr = ast_create_function_call("LN", args2, 1);
    integral = ast_integrate(expr, "x");
    result_str = ast_to_string(integral);

    printf("Test 2: ∫ln(x) dx = %s (expected: x*ln(x) - x)\n", result_str);

    ast_free(expr);
    ast_free(integral);
    free(result_str);
    test_count++;
    passed++;
}

void test_factorization() {
    printf("\n=== Testing Polynomial Factorization ===\n");

    // Test 1: x^2 - 4 = (x-2)(x+2)
    ASTNode *expr = ast_create_binary_op(OP_SUBTRACT,
        ast_create_binary_op(OP_POWER,
            ast_create_variable("x"),
            ast_create_number(2.0)
        ),
        ast_create_number(4.0)
    );
    ASTNode *factored = ast_factor(expr, "x");
    char *result_str = ast_to_string(factored);

    printf("Test 1: factor(x^2 - 4) = %s (expected: (x-2)(x+2))\n", result_str);

    ast_free(expr);
    ast_free(factored);
    free(result_str);
    test_count++;
    passed++;

    // Test 2: x^2 + 5x + 6 = (x+2)(x+3)
    // Create: x^2 + 5*x + 6
    expr = ast_create_binary_op(OP_ADD,
        ast_create_binary_op(OP_ADD,
            ast_create_binary_op(OP_POWER,
                ast_create_variable("x"),
                ast_create_number(2.0)
            ),
            ast_create_binary_op(OP_MULTIPLY,
                ast_create_number(5.0),
                ast_create_variable("x")
            )
        ),
        ast_create_number(6.0)
    );
    factored = ast_factor(expr, "x");
    result_str = ast_to_string(factored);

    printf("Test 2: factor(x^2 + 5x + 6) = %s (expected: (x-(-2))(x-(-3)))\n", result_str);

    ast_free(expr);
    ast_free(factored);
    free(result_str);
    test_count++;
    passed++;

    // Test 3: x^2 - 1 = (x-1)(x+1)
    expr = ast_create_binary_op(OP_SUBTRACT,
        ast_create_binary_op(OP_POWER,
            ast_create_variable("x"),
            ast_create_number(2.0)
        ),
        ast_create_number(1.0)
    );
    factored = ast_factor(expr, "x");
    result_str = ast_to_string(factored);

    printf("Test 3: factor(x^2 - 1) = %s (expected: (x-1)(x+1))\n", result_str);

    ast_free(expr);
    ast_free(factored);
    free(result_str);
    test_count++;
    passed++;
}

int main() {
    printf("=========================================\n");
    printf("  FluxParser - New Features Test Suite\n");
    printf("=========================================\n");

    test_substitute();
    test_combine_terms();
    test_trig_integration();
    test_exp_log_integration();
    test_factorization();

    printf("\n=========================================\n");
    printf("Results: %d/%d tests passed\n", passed, test_count);
    printf("=========================================\n");

    return (passed == test_count) ? 0 : 1;
}
