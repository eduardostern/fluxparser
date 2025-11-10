#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

void print_separator() {
    printf("\n========================================\n\n");
}

/* Test 1: Basic Integration */
void test_basic_integration() {
    printf("=== TEST 1: Basic Integration ===\n\n");

    /* ∫5 dx = 5x */
    printf("1. ∫5 dx = 5x\n");
    ASTNode *five = ast_create_number(5.0);
    ASTNode *integral1 = ast_integrate(five, "x");
    ASTNode *simplified1 = ast_simplify(integral1);
    char *result1 = ast_to_string(simplified1);
    printf("   Result: %s\n\n", result1);
    free(result1);
    ast_free(five);
    ast_free(simplified1);

    /* ∫x dx = x^2/2 */
    printf("2. ∫x dx = x^2/2\n");
    ASTNode *x = ast_create_variable("x");
    ASTNode *integral2 = ast_integrate(x, "x");
    char *result2 = ast_to_string(integral2);
    printf("   Result: %s\n\n", result2);
    free(result2);
    ast_free(x);
    ast_free(integral2);

    /* ∫x^2 dx = x^3/3 */
    printf("3. ∫x^2 dx = x^3/3\n");
    ASTNode *x3 = ast_create_variable("x");
    ASTNode *two = ast_create_number(2.0);
    ASTNode *x_squared = ast_create_binary_op(OP_POWER, x3, two);
    ASTNode *integral3 = ast_integrate(x_squared, "x");
    char *result3 = ast_to_string(integral3);
    printf("   Result: %s\n\n", result3);
    free(result3);
    ast_free(x_squared);
    ast_free(integral3);

    /* ∫(3x + 5) dx = 3x^2/2 + 5x */
    printf("4. ∫(3x + 5) dx = 3x^2/2 + 5x\n");
    ASTNode *three = ast_create_number(3.0);
    ASTNode *x4 = ast_create_variable("x");
    ASTNode *three_x = ast_create_binary_op(OP_MULTIPLY, three, x4);
    ASTNode *five2 = ast_create_number(5.0);
    ASTNode *sum = ast_create_binary_op(OP_ADD, three_x, five2);
    ASTNode *integral4 = ast_integrate(sum, "x");
    char *result4 = ast_to_string(integral4);
    printf("   Result: %s\n\n", result4);
    free(result4);
    ast_free(sum);
    ast_free(integral4);

    print_separator();
}

/* Test 2: Trig Function Integration */
void test_trig_integration() {
    printf("=== TEST 2: Trigonometric Integration ===\n\n");

    /* ∫sin(x) dx = -cos(x) */
    printf("1. ∫sin(x) dx = -cos(x)\n");
    ASTNode *x1 = ast_create_variable("x");
    ASTNode *args1[] = {x1};
    ASTNode *sin_x = ast_create_function_call("SIN", args1, 1);
    ASTNode *integral1 = ast_integrate(sin_x, "x");
    char *result1 = ast_to_string(integral1);
    printf("   Result: %s\n\n", result1);
    free(result1);
    ast_free(sin_x);
    ast_free(integral1);

    /* ∫cos(x) dx = sin(x) */
    printf("2. ∫cos(x) dx = sin(x)\n");
    ASTNode *x2 = ast_create_variable("x");
    ASTNode *args2[] = {x2};
    ASTNode *cos_x = ast_create_function_call("COS", args2, 1);
    ASTNode *integral2 = ast_integrate(cos_x, "x");
    char *result2 = ast_to_string(integral2);
    printf("   Result: %s\n\n", result2);
    free(result2);
    ast_free(cos_x);
    ast_free(integral2);

    print_separator();
}

/* Test 3: Exponential & Log Integration */
void test_exp_log_integration() {
    printf("=== TEST 3: Exponential & Logarithmic Integration ===\n\n");

    /* ∫e^x dx = e^x */
    printf("1. ∫e^x dx = e^x\n");
    ASTNode *x1 = ast_create_variable("x");
    ASTNode *args1[] = {x1};
    ASTNode *exp_x = ast_create_function_call("EXP", args1, 1);
    ASTNode *integral1 = ast_integrate(exp_x, "x");
    char *result1 = ast_to_string(integral1);
    printf("   Result: %s\n\n", result1);
    free(result1);
    ast_free(exp_x);
    ast_free(integral1);

    /* ∫ln(x) dx = x*ln(x) - x */
    printf("2. ∫ln(x) dx = x*ln(x) - x\n");
    ASTNode *x2 = ast_create_variable("x");
    ASTNode *args2[] = {x2};
    ASTNode *ln_x = ast_create_function_call("LN", args2, 1);
    ASTNode *integral2 = ast_integrate(ln_x, "x");
    char *result2 = ast_to_string(integral2);
    printf("   Result: %s\n\n", result2);
    free(result2);
    ast_free(ln_x);
    ast_free(integral2);

    /* ∫1/x dx = ln(x) */
    printf("3. ∫1/x dx = ln(x)\n");
    ASTNode *one = ast_create_number(1.0);
    ASTNode *x3 = ast_create_variable("x");
    ASTNode *one_over_x = ast_create_binary_op(OP_DIVIDE, one, x3);
    ASTNode *integral3 = ast_integrate(one_over_x, "x");
    char *result3 = ast_to_string(integral3);
    printf("   Result: %s\n\n", result3);
    free(result3);
    ast_free(one_over_x);
    ast_free(integral3);

    print_separator();
}

/* Test 4: Linear Equation Solving */
void test_linear_solving() {
    printf("=== TEST 4: Linear Equation Solving ===\n\n");

    /* Solve: 2x + 3 = 0 → x = -1.5 */
    printf("1. Solve 2x + 3 = 0\n");
    ASTNode *two = ast_create_number(2.0);
    ASTNode *x1 = ast_create_variable("x");
    ASTNode *two_x = ast_create_binary_op(OP_MULTIPLY, two, x1);
    ASTNode *three = ast_create_number(3.0);
    ASTNode *eq1 = ast_create_binary_op(OP_ADD, two_x, three);

    SolveResult result1 = ast_solve_equation(eq1, "x");
    if (result1.has_solution) {
        for (int i = 0; i < result1.solution_count; i++) {
            char *sol = ast_to_string(result1.solutions[i]);
            printf("   Solution %d: x = %s\n", i+1, sol);
            free(sol);
        }
    } else {
        printf("   %s\n", result1.error_message);
    }
    printf("\n");
    solve_result_free(&result1);
    ast_free(eq1);

    /* Solve: 5x - 10 = 0 → x = 2 */
    printf("2. Solve 5x - 10 = 0\n");
    ASTNode *five = ast_create_number(5.0);
    ASTNode *x2 = ast_create_variable("x");
    ASTNode *five_x = ast_create_binary_op(OP_MULTIPLY, five, x2);
    ASTNode *ten = ast_create_number(10.0);
    ASTNode *eq2 = ast_create_binary_op(OP_SUBTRACT, five_x, ten);

    SolveResult result2 = ast_solve_equation(eq2, "x");
    if (result2.has_solution) {
        for (int i = 0; i < result2.solution_count; i++) {
            char *sol = ast_to_string(result2.solutions[i]);
            printf("   Solution %d: x = %s\n", i+1, sol);
            free(sol);
        }
    } else {
        printf("   %s\n", result2.error_message);
    }
    printf("\n");
    solve_result_free(&result2);
    ast_free(eq2);

    print_separator();
}

/* Test 5: Quadratic Equation Solving */
void test_quadratic_solving() {
    printf("=== TEST 5: Quadratic Equation Solving ===\n\n");

    /* Solve: x^2 - 5x + 6 = 0 → x = 2, x = 3 */
    printf("1. Solve x^2 - 5x + 6 = 0\n");
    ASTNode *x1 = ast_create_variable("x");
    ASTNode *two1 = ast_create_number(2.0);
    ASTNode *x_squared = ast_create_binary_op(OP_POWER, x1, two1);

    ASTNode *five = ast_create_number(5.0);
    ASTNode *x2 = ast_create_variable("x");
    ASTNode *five_x = ast_create_binary_op(OP_MULTIPLY, five, x2);

    ASTNode *six = ast_create_number(6.0);

    ASTNode *sub1 = ast_create_binary_op(OP_SUBTRACT, x_squared, five_x);
    ASTNode *eq1 = ast_create_binary_op(OP_ADD, sub1, six);

    SolveResult result1 = ast_solve_equation(eq1, "x");
    if (result1.has_solution) {
        for (int i = 0; i < result1.solution_count; i++) {
            char *sol = ast_to_string(result1.solutions[i]);
            printf("   Solution %d: x = %s\n", i+1, sol);
            free(sol);
        }
    } else {
        printf("   %s\n", result1.error_message);
    }
    printf("   Expected: x = 2, x = 3\n\n");
    solve_result_free(&result1);
    ast_free(eq1);

    /* Solve: x^2 - 4 = 0 → x = ±2 */
    printf("2. Solve x^2 - 4 = 0\n");
    ASTNode *x3 = ast_create_variable("x");
    ASTNode *two2 = ast_create_number(2.0);
    ASTNode *x_sq2 = ast_create_binary_op(OP_POWER, x3, two2);
    ASTNode *four = ast_create_number(4.0);
    ASTNode *eq2 = ast_create_binary_op(OP_SUBTRACT, x_sq2, four);

    SolveResult result2 = ast_solve_equation(eq2, "x");
    if (result2.has_solution) {
        for (int i = 0; i < result2.solution_count; i++) {
            char *sol = ast_to_string(result2.solutions[i]);
            printf("   Solution %d: x = %s\n", i+1, sol);
            free(sol);
        }
    } else {
        printf("   %s\n", result2.error_message);
    }
    printf("   Expected: x = -2, x = 2\n\n");
    solve_result_free(&result2);
    ast_free(eq2);

    /* Solve: x^2 + 2x + 1 = 0 → x = -1 (double root) */
    printf("3. Solve x^2 + 2x + 1 = 0 (double root)\n");
    ASTNode *x4 = ast_create_variable("x");
    ASTNode *two3 = ast_create_number(2.0);
    ASTNode *x_sq3 = ast_create_binary_op(OP_POWER, x4, two3);

    ASTNode *two4 = ast_create_number(2.0);
    ASTNode *x5 = ast_create_variable("x");
    ASTNode *two_x = ast_create_binary_op(OP_MULTIPLY, two4, x5);

    ASTNode *one = ast_create_number(1.0);

    ASTNode *sum1 = ast_create_binary_op(OP_ADD, x_sq3, two_x);
    ASTNode *eq3 = ast_create_binary_op(OP_ADD, sum1, one);

    SolveResult result3 = ast_solve_equation(eq3, "x");
    if (result3.has_solution) {
        for (int i = 0; i < result3.solution_count; i++) {
            char *sol = ast_to_string(result3.solutions[i]);
            printf("   Solution %d: x = %s\n", i+1, sol);
            free(sol);
        }
    } else {
        printf("   %s\n", result3.error_message);
    }
    printf("   Expected: x = -1\n\n");
    solve_result_free(&result3);
    ast_free(eq3);

    print_separator();
}

/* Test 6: Integration and Differentiation Together */
void test_calculus_roundtrip() {
    printf("=== TEST 6: Calculus Round-Trip ===\n\n");

    /* f(x) = x^2 */
    printf("1. f(x) = x^2\n");
    ASTNode *x = ast_create_variable("x");
    ASTNode *two = ast_create_number(2.0);
    ASTNode *f = ast_create_binary_op(OP_POWER, x, two);

    char *f_str = ast_to_string(f);
    printf("   f(x)  = %s\n", f_str);
    free(f_str);

    /* f'(x) = 2x */
    ASTNode *f_prime = ast_differentiate(f, "x");
    ASTNode *f_prime_simp = ast_simplify(f_prime);
    char *fp_str = ast_to_string(f_prime_simp);
    printf("   f'(x) = %s\n", fp_str);
    free(fp_str);

    /* ∫f'(x) dx should be close to f(x) */
    ASTNode *integral = ast_integrate(f_prime_simp, "x");
    ASTNode *integral_simp = ast_simplify(integral);
    char *int_str = ast_to_string(integral_simp);
    printf("   ∫f'(x) dx = %s (should be x^2)\n\n", int_str);
    free(int_str);

    ast_free(f);
    ast_free(f_prime_simp);
    ast_free(integral_simp);

    print_separator();
}

int main() {
    printf("\n");
    printf("==============================================\n");
    printf("  CALCULUS FEATURES TEST SUITE\n");
    printf("  Integration & Equation Solving\n");
    printf("==============================================\n");
    print_separator();

    test_basic_integration();
    test_trig_integration();
    test_exp_log_integration();
    test_linear_solving();
    test_quadratic_solving();
    test_calculus_roundtrip();

    printf("==============================================\n");
    printf("All calculus tests completed!\n");
    printf("==============================================\n\n");

    return 0;
}
