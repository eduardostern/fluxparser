/*
 * FluxParser - Research-Grade C Math Parser
 * Copyright (C) 2025 Eduardo Stern
 *
 * Dual Licensed:
 * - GPL-3.0 for open-source/non-commercial use
 * - Commercial license available - see LICENSE-COMMERCIAL.md
 *
 * SPDX-License-Identifier: GPL-3.0-or-later OR Commercial
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "ast.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void print_separator() {
    printf("\n========================================\n\n");
}

void print_result(const char *equation_str, NumericalSolveResult result, double expected) {
    (void)equation_str;  /* Suppress unused parameter warning */
    if (result.converged) {
        double error = fabs(result.solution - expected);
        printf("   ✓ Converged in %d iterations\n", result.iterations);
        printf("   Solution: x = %.6f (expected %.6f)\n", result.solution, expected);
        printf("   Error: %.2e (final f(x) = %.2e)\n", error, result.final_error);
        if (error < 0.01) {
            printf("   [PASS]\n");
        } else {
            printf("   [WARN] Solution differs from expected\n");
        }
    } else {
        printf("   ✗ Failed to converge\n");
        printf("   Message: %s\n", result.error_message);
        printf("   Best guess: x = %.6f (after %d iterations)\n",
               result.solution, result.iterations);
        printf("   [FAIL]\n");
    }
}

/* Test 1: Trigonometric Equations */
void test_trig_equations() {
    printf("=== TEST 1: Trigonometric Equations ===\n\n");

    /* sin(x) = 0.5, solve for x in [0, π/2] */
    printf("1. sin(x) - 0.5 = 0  (solution: x = π/6 ≈ 0.524)\n");
    ASTNode *x1 = ast_create_variable("x");
    ASTNode *args1[] = {x1};
    ASTNode *sin_x = ast_create_function_call("SIN", args1, 1);
    ASTNode *half = ast_create_number(0.5);
    ASTNode *eq1 = ast_create_binary_op(OP_SUBTRACT, sin_x, half);

    NumericalSolveResult result1 = ast_solve_numerical(eq1, "x", 0.5, 1e-6, 100);
    print_result("sin(x) - 0.5", result1, M_PI / 6.0);
    printf("\n");
    ast_free(eq1);

    /* cos(x) = 0, solve for x near π/2 */
    printf("2. cos(x) = 0  (solution: x = π/2 ≈ 1.571)\n");
    ASTNode *x2 = ast_create_variable("x");
    ASTNode *args2[] = {x2};
    ASTNode *cos_x = ast_create_function_call("COS", args2, 1);

    NumericalSolveResult result2 = ast_solve_numerical(cos_x, "x", 1.5, 1e-6, 100);
    print_result("cos(x)", result2, M_PI / 2.0);
    printf("\n");
    ast_free(cos_x);

    /* tan(x) = 1, solve for x (solution: π/4) */
    printf("3. tan(x) - 1 = 0  (solution: x = π/4 ≈ 0.785)\n");
    ASTNode *x3 = ast_create_variable("x");
    ASTNode *args3[] = {x3};
    ASTNode *tan_x = ast_create_function_call("TAN", args3, 1);
    ASTNode *one = ast_create_number(1.0);
    ASTNode *eq3 = ast_create_binary_op(OP_SUBTRACT, tan_x, one);

    NumericalSolveResult result3 = ast_solve_numerical(eq3, "x", 0.7, 1e-6, 100);
    print_result("tan(x) - 1", result3, M_PI / 4.0);
    printf("\n");
    ast_free(eq3);

    print_separator();
}

/* Test 2: Exponential Equations */
void test_exponential_equations() {
    printf("=== TEST 2: Exponential Equations ===\n\n");

    /* e^x = 5, solve for x (solution: ln(5) ≈ 1.609) */
    printf("1. e^x - 5 = 0  (solution: x = ln(5) ≈ 1.609)\n");
    ASTNode *x1 = ast_create_variable("x");
    ASTNode *args1[] = {x1};
    ASTNode *exp_x = ast_create_function_call("EXP", args1, 1);
    ASTNode *five = ast_create_number(5.0);
    ASTNode *eq1 = ast_create_binary_op(OP_SUBTRACT, exp_x, five);

    NumericalSolveResult result1 = ast_solve_numerical(eq1, "x", 1.0, 1e-6, 100);
    print_result("e^x - 5", result1, logf(5.0));
    printf("\n");
    ast_free(eq1);

    /* e^x = x + 2, solve for x (solution: ≈ 1.146) */
    printf("2. e^x - x - 2 = 0  (solution: x ≈ 1.146)\n");
    ASTNode *x2 = ast_create_variable("x");
    ASTNode *args2[] = {x2};
    ASTNode *exp_x2 = ast_create_function_call("EXP", args2, 1);
    ASTNode *x2_copy = ast_create_variable("x");
    ASTNode *two = ast_create_number(2.0);
    ASTNode *sub1 = ast_create_binary_op(OP_SUBTRACT, exp_x2, x2_copy);
    ASTNode *eq2 = ast_create_binary_op(OP_SUBTRACT, sub1, two);

    NumericalSolveResult result2 = ast_solve_numerical(eq2, "x", 1.0, 1e-6, 100);
    print_result("e^x - x - 2", result2, 1.146193);
    printf("\n");
    ast_free(eq2);

    print_separator();
}

/* Test 3: Logarithmic Equations */
void test_logarithmic_equations() {
    printf("=== TEST 3: Logarithmic Equations ===\n\n");

    /* ln(x) = 2, solve for x (solution: e^2 ≈ 7.389) */
    printf("1. ln(x) - 2 = 0  (solution: x = e^2 ≈ 7.389)\n");
    ASTNode *x1 = ast_create_variable("x");
    ASTNode *args1[] = {x1};
    ASTNode *ln_x = ast_create_function_call("LN", args1, 1);
    ASTNode *two = ast_create_number(2.0);
    ASTNode *eq1 = ast_create_binary_op(OP_SUBTRACT, ln_x, two);

    NumericalSolveResult result1 = ast_solve_numerical(eq1, "x", 5.0, 1e-6, 100);
    print_result("ln(x) - 2", result1, expf(2.0));
    printf("\n");
    ast_free(eq1);

    /* ln(x) = x - 2, solve for x (solution: ≈ 2.120) */
    printf("2. ln(x) - x + 2 = 0  (solution: x ≈ 2.120)\n");
    ASTNode *x2 = ast_create_variable("x");
    ASTNode *args2[] = {x2};
    ASTNode *ln_x2 = ast_create_function_call("LN", args2, 1);
    ASTNode *x2_copy = ast_create_variable("x");
    ASTNode *two2 = ast_create_number(2.0);
    ASTNode *sub1 = ast_create_binary_op(OP_SUBTRACT, ln_x2, x2_copy);
    ASTNode *eq2 = ast_create_binary_op(OP_ADD, sub1, two2);

    NumericalSolveResult result2 = ast_solve_numerical(eq2, "x", 2.0, 1e-6, 100);
    print_result("ln(x) - x + 2", result2, 2.120065);
    printf("\n");
    ast_free(eq2);

    print_separator();
}

/* Test 4: Polynomial Equations (compare with symbolic solver) */
void test_polynomial_equations() {
    printf("=== TEST 4: Polynomial Equations (vs Symbolic) ===\n\n");

    /* x^2 - 5 = 0, solve for x (solution: √5 ≈ 2.236) */
    printf("1. x^2 - 5 = 0  (solution: x = √5 ≈ 2.236)\n");
    ASTNode *x1 = ast_create_variable("x");
    ASTNode *two = ast_create_number(2.0);
    ASTNode *x_squared = ast_create_binary_op(OP_POWER, x1, two);
    ASTNode *five = ast_create_number(5.0);
    ASTNode *eq1 = ast_create_binary_op(OP_SUBTRACT, x_squared, five);

    printf("   Numerical method:\n");
    NumericalSolveResult num_result = ast_solve_numerical(eq1, "x", 2.0, 1e-6, 100);
    print_result("x^2 - 5", num_result, sqrtf(5.0));

    printf("\n   Symbolic method:\n");
    SolveResult sym_result = ast_solve_equation(eq1, "x");
    if (sym_result.has_solution) {
        for (int i = 0; i < sym_result.solution_count; i++) {
            float sol = ast_evaluate(sym_result.solutions[i], NULL);
            printf("   Solution %d: x = %.6f\n", i+1, sol);
        }
        solve_result_free(&sym_result);
    }
    printf("\n");
    ast_free(eq1);

    /* x^3 - 8 = 0, solve for x (solution: 2) */
    printf("2. x^3 - 8 = 0  (solution: x = 2)\n");
    ASTNode *x2 = ast_create_variable("x");
    ASTNode *three = ast_create_number(3.0);
    ASTNode *x_cubed = ast_create_binary_op(OP_POWER, x2, three);
    ASTNode *eight = ast_create_number(8.0);
    ASTNode *eq2 = ast_create_binary_op(OP_SUBTRACT, x_cubed, eight);

    printf("   Numerical method:\n");
    NumericalSolveResult num_result2 = ast_solve_numerical(eq2, "x", 1.5, 1e-6, 100);
    print_result("x^3 - 8", num_result2, 2.0);

    printf("\n   Symbolic method: Not supported (cubic equation)\n");
    printf("\n");
    ast_free(eq2);

    print_separator();
}

/* Test 5: Mixed Transcendental Equations */
void test_mixed_equations() {
    printf("=== TEST 5: Mixed Transcendental Equations ===\n\n");

    /* x*sin(x) = 1, solve for x (solution: ≈ 1.114) */
    printf("1. x*sin(x) - 1 = 0  (solution: x ≈ 1.114)\n");
    ASTNode *x1 = ast_create_variable("x");
    ASTNode *x1_copy = ast_create_variable("x");
    ASTNode *args1[] = {x1_copy};
    ASTNode *sin_x = ast_create_function_call("SIN", args1, 1);
    ASTNode *x_sin_x = ast_create_binary_op(OP_MULTIPLY, x1, sin_x);
    ASTNode *one = ast_create_number(1.0);
    ASTNode *eq1 = ast_create_binary_op(OP_SUBTRACT, x_sin_x, one);

    NumericalSolveResult result1 = ast_solve_numerical(eq1, "x", 1.0, 1e-6, 100);
    print_result("x*sin(x) - 1", result1, 1.114157);
    printf("\n");
    ast_free(eq1);

    /* sqrt(x) = cos(x), solve for x (solution: ≈ 0.641) */
    printf("2. sqrt(x) - cos(x) = 0  (solution: x ≈ 0.641)\n");
    ASTNode *x2 = ast_create_variable("x");
    ASTNode *args2a[] = {x2};
    ASTNode *sqrt_x = ast_create_function_call("SQRT", args2a, 1);
    ASTNode *x2_copy = ast_create_variable("x");
    ASTNode *args2b[] = {x2_copy};
    ASTNode *cos_x = ast_create_function_call("COS", args2b, 1);
    ASTNode *eq2 = ast_create_binary_op(OP_SUBTRACT, sqrt_x, cos_x);

    NumericalSolveResult result2 = ast_solve_numerical(eq2, "x", 0.5, 1e-6, 100);
    print_result("sqrt(x) - cos(x)", result2, 0.641186);
    printf("\n");
    ast_free(eq2);

    print_separator();
}

/* Test 6: Convergence Behavior */
void test_convergence() {
    printf("=== TEST 6: Convergence Behavior ===\n\n");

    /* Good convergence: x^2 - 4 = 0 from x0 = 1 */
    printf("1. Fast convergence: x^2 - 4 = 0  (from x0 = 1)\n");
    ASTNode *x1 = ast_create_variable("x");
    ASTNode *two = ast_create_number(2.0);
    ASTNode *x_squared = ast_create_binary_op(OP_POWER, x1, two);
    ASTNode *four = ast_create_number(4.0);
    ASTNode *eq1 = ast_create_binary_op(OP_SUBTRACT, x_squared, four);

    NumericalSolveResult result1 = ast_solve_numerical(eq1, "x", 1.0, 1e-6, 100);
    print_result("x^2 - 4", result1, 2.0);
    printf("\n");
    ast_free(eq1);

    /* Slow convergence: ln(x) - 1 = 0 from x0 = 1 */
    printf("2. Moderate convergence: ln(x) - 1 = 0  (from x0 = 1)\n");
    ASTNode *x2 = ast_create_variable("x");
    ASTNode *args2[] = {x2};
    ASTNode *ln_x = ast_create_function_call("LN", args2, 1);
    ASTNode *one = ast_create_number(1.0);
    ASTNode *eq2 = ast_create_binary_op(OP_SUBTRACT, ln_x, one);

    NumericalSolveResult result2 = ast_solve_numerical(eq2, "x", 1.0, 1e-6, 100);
    print_result("ln(x) - 1", result2, expf(1.0));
    printf("\n");
    ast_free(eq2);

    print_separator();
}

int main() {
    printf("\n");
    printf("==============================================\n");
    printf("  NUMERICAL SOLVER TEST SUITE\n");
    printf("  Newton-Raphson Method\n");
    printf("==============================================\n");
    print_separator();

    test_trig_equations();
    test_exponential_equations();
    test_logarithmic_equations();
    test_polynomial_equations();
    test_mixed_equations();
    test_convergence();

    printf("==============================================\n");
    printf("All numerical solver tests completed!\n");
    printf("==============================================\n\n");

    return 0;
}
