/*
 * FluxParser - Research-Grade C Math Parser
 * Copyright (C) 2025 Eduardo Stern
 *
 * Test Suite for Advanced Features:
 * - Numerical Integration (Trapezoidal & Simpson's)
 * - Partial Derivatives & Gradient
 * - Taylor Series Expansion
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "parser.h"
#include "ast.h"

#define ASSERT_CLOSE(a, b, tol) do { \
    double diff = fabs((a) - (b)); \
    if (diff > (tol)) { \
        printf("  ❌ FAILED: Expected %.10f, got %.10f (diff: %.2e)\n", (double)(b), (double)(a), diff); \
        return 0; \
    } \
} while(0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        printf("  ❌ FAILED: Expression was false\n"); \
        return 0; \
    } \
} while(0)

int test_passed = 0;
int test_failed = 0;

void print_section(const char *title) {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║ %-62s ║\n", title);
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
}

void print_test(const char *name) {
    printf("Testing: %s\n", name);
}

void test_result(int passed) {
    if (passed) {
        printf("  ✅ PASSED\n\n");
        test_passed++;
    } else {
        printf("\n");
        test_failed++;
    }
}

/* ============================================================================
 * NUMERICAL INTEGRATION TESTS
 * ============================================================================ */

int test_integrate_polynomial() {
    print_test("Numerical integration of x^2 from 0 to 1");

    // ∫₀¹ x² dx = [x³/3]₀¹ = 1/3 ≈ 0.333333
    double expected = 1.0/3.0;

    // Build AST for x^2
    ASTNode *x = ast_create_variable("x");
    ASTNode *expr = ast_create_binary_op(OP_POWER, x, ast_create_number(2.0));

    // Test trapezoidal rule
    double result_trap = ast_integrate_numerical_trapezoidal(expr, "x", 0.0, 1.0, 1000);
    printf("  Trapezoidal (1000 steps): %.10f\n", result_trap);
    ASSERT_CLOSE(result_trap, expected, 1e-6);

    // Test Simpson's rule (should be exact for polynomials)
    double result_simp = ast_integrate_numerical_simpson(expr, "x", 0.0, 1.0, 100);
    printf("  Simpson's (100 steps):    %.10f\n", result_simp);
    ASSERT_CLOSE(result_simp, expected, 1e-9);

    ast_free(expr);
    printf("  Expected: %.10f\n", expected);
    return 1;
}

int test_integrate_sin() {
    print_test("Numerical integration of sin(x) from 0 to π");

    // ∫₀^π sin(x) dx = [-cos(x)]₀^π = -cos(π) + cos(0) = 1 + 1 = 2
    double expected = 2.0;
    double pi = M_PI;

    // Build AST for sin(x)
    ASTNode *x = ast_create_variable("x");
    ASTNode *args[] = {x};
    ASTNode *expr = ast_create_function_call("SIN", args, 1);

    // Test with different methods
    double result_trap = ast_integrate_numerical_trapezoidal(expr, "x", 0.0, pi, 1000);
    printf("  Trapezoidal (1000 steps): %.10f\n", result_trap);
    ASSERT_CLOSE(result_trap, expected, 1e-5);

    double result_simp = ast_integrate_numerical_simpson(expr, "x", 0.0, pi, 100);
    printf("  Simpson's (100 steps):    %.10f\n", result_simp);
    ASSERT_CLOSE(result_simp, expected, 1e-7);

    ast_free(expr);
    printf("  Expected: %.10f\n", expected);
    return 1;
}

int test_integrate_exponential() {
    print_test("Numerical integration of e^x from 0 to 1");

    // ∫₀¹ e^x dx = [e^x]₀¹ = e - 1 ≈ 1.718281828
    double expected = exp(1.0) - 1.0;

    // Build AST for exp(x)
    ASTNode *x = ast_create_variable("x");
    ASTNode *args[] = {x};
    ASTNode *expr = ast_create_function_call("EXP", args, 1);

    double result_simp = ast_integrate_numerical_simpson(expr, "x", 0.0, 1.0, 100);
    printf("  Simpson's (100 steps): %.10f\n", result_simp);
    printf("  Expected:              %.10f\n", expected);
    ASSERT_CLOSE(result_simp, expected, 1e-8);

    ast_free(expr);
    return 1;
}

int test_integrate_normal_distribution() {
    print_test("Numerical integration of standard normal PDF from -1 to 1");

    // ∫₋₁¹ (1/√(2π)) * e^(-x²/2) dx ≈ 0.682689492 (68.3% within 1σ)
    // For simplicity, we'll test e^(-x²) which is proportional
    // ∫₋₁¹ e^(-x²) dx ≈ 1.49365

    double expected = 1.49365;  // Numerical value

    // Build AST for exp(-x^2)
    ASTNode *x = ast_create_variable("x");
    ASTNode *x_sq = ast_create_binary_op(OP_POWER, x, ast_create_number(2.0));
    ASTNode *neg_x_sq = ast_create_unary_op(OP_NEGATE, x_sq);
    ASTNode *args[] = {neg_x_sq};
    ASTNode *expr = ast_create_function_call("EXP", args, 1);

    double result = ast_integrate_numerical_simpson(expr, "x", -1.0, 1.0, 200);
    printf("  Result:   %.10f\n", result);
    printf("  Expected: %.10f (approximate)\n", expected);
    ASSERT_CLOSE(result, expected, 1e-4);

    ast_free(expr);
    return 1;
}

/* ============================================================================
 * PARTIAL DERIVATIVES & GRADIENT TESTS
 * ============================================================================ */

int test_partial_derivative_simple() {
    print_test("Partial derivative of x^2 + y^2 with respect to x");

    // f(x,y) = x² + y²
    // ∂f/∂x = 2x

    ASTNode *x = ast_create_variable("x");
    ASTNode *y = ast_create_variable("y");
    ASTNode *x_sq = ast_create_binary_op(OP_POWER, x, ast_create_number(2.0));
    ASTNode *y_sq = ast_create_binary_op(OP_POWER, y, ast_create_number(2.0));
    ASTNode *expr = ast_create_binary_op(OP_ADD, x_sq, y_sq);

    // Compute ∂f/∂x
    ASTNode *partial_x = ast_partial_derivative(expr, "x");
    char *partial_str = ast_to_string(partial_x);
    printf("  ∂f/∂x = %s\n", partial_str);

    // Evaluate at x=3, y=4
    double values[] = {3.0, 4.0};
    VarMapping mappings[] = {{"x", 0}, {"y", 1}};
    VarContext ctx = {
        .values = values,
        .count = 2,
        .mappings = mappings,
        .mapping_count = 2
    };

    double result = ast_evaluate(partial_x, &ctx);
    printf("  At (x=3, y=4): %.10f\n", result);
    printf("  Expected: 6.0 (2*x = 2*3)\n", result);
    ASSERT_CLOSE(result, 6.0, 1e-10);

    free(partial_str);
    ast_free(expr);
    ast_free(partial_x);
    return 1;
}

int test_gradient_2d() {
    print_test("Gradient of f(x,y) = x²y + xy²");

    // f(x,y) = x²y + xy²
    // ∂f/∂x = 2xy + y²
    // ∂f/∂y = x² + 2xy
    // At (2,3): ∂f/∂x = 2(2)(3) + 9 = 21, ∂f/∂y = 4 + 12 = 16

    // Build f(x,y) = x²y + xy²
    ASTNode *x = ast_create_variable("x");
    ASTNode *y = ast_create_variable("y");
    ASTNode *x_sq = ast_create_binary_op(OP_POWER,
        ast_create_variable("x"), ast_create_number(2.0));
    ASTNode *y_sq = ast_create_binary_op(OP_POWER,
        ast_create_variable("y"), ast_create_number(2.0));
    ASTNode *x_sq_y = ast_create_binary_op(OP_MULTIPLY, x_sq, ast_create_variable("y"));
    ASTNode *x_y_sq = ast_create_binary_op(OP_MULTIPLY, ast_create_variable("x"), y_sq);
    ASTNode *expr = ast_create_binary_op(OP_ADD, x_sq_y, x_y_sq);

    // Compute gradient
    const char *vars[] = {"x", "y"};
    Gradient grad = ast_gradient(expr, vars, 2);

    printf("  Gradient components:\n");
    for (int i = 0; i < grad.count; i++) {
        char *comp_str = ast_to_string(grad.components[i]);
        printf("    ∂f/∂%s = %s\n", grad.var_names[i], comp_str);
        free(comp_str);
    }

    // Evaluate gradient at (2,3)
    double values[] = {2.0, 3.0};
    VarMapping mappings[] = {{"x", 0}, {"y", 1}};
    VarContext ctx = {
        .values = values,
        .count = 2,
        .mappings = mappings,
        .mapping_count = 2
    };

    double *grad_values = gradient_evaluate(&grad, &ctx);
    printf("  ∇f(2,3) = [%.2f, %.2f]\n", grad_values[0], grad_values[1]);
    printf("  Expected: [21.0, 16.0]\n");

    ASSERT_CLOSE(grad_values[0], 21.0, 1e-9);
    ASSERT_CLOSE(grad_values[1], 16.0, 1e-9);

    free(grad_values);
    gradient_free(&grad);
    ast_free(expr);
    return 1;
}

int test_gradient_3d() {
    print_test("Gradient of f(x,y,z) = x² + y² + z²");

    // f(x,y,z) = x² + y² + z²
    // ∇f = [2x, 2y, 2z]
    // At (1,2,3): ∇f = [2, 4, 6]

    ASTNode *x_sq = ast_create_binary_op(OP_POWER,
        ast_create_variable("x"), ast_create_number(2.0));
    ASTNode *y_sq = ast_create_binary_op(OP_POWER,
        ast_create_variable("y"), ast_create_number(2.0));
    ASTNode *z_sq = ast_create_binary_op(OP_POWER,
        ast_create_variable("z"), ast_create_number(2.0));
    ASTNode *xy = ast_create_binary_op(OP_ADD, x_sq, y_sq);
    ASTNode *expr = ast_create_binary_op(OP_ADD, xy, z_sq);

    const char *vars[] = {"x", "y", "z"};
    Gradient grad = ast_gradient(expr, vars, 3);

    // Evaluate at (1,2,3)
    double values[] = {1.0, 2.0, 3.0};
    VarMapping mappings[] = {{"x", 0}, {"y", 1}, {"z", 2}};
    VarContext ctx = {
        .values = values,
        .count = 3,
        .mappings = mappings,
        .mapping_count = 3
    };

    double *grad_values = gradient_evaluate(&grad, &ctx);
    printf("  ∇f(1,2,3) = [%.2f, %.2f, %.2f]\n",
           grad_values[0], grad_values[1], grad_values[2]);
    printf("  Expected:   [2.0, 4.0, 6.0]\n");

    ASSERT_CLOSE(grad_values[0], 2.0, 1e-9);
    ASSERT_CLOSE(grad_values[1], 4.0, 1e-9);
    ASSERT_CLOSE(grad_values[2], 6.0, 1e-9);

    free(grad_values);
    gradient_free(&grad);
    ast_free(expr);
    return 1;
}

/* ============================================================================
 * TAYLOR SERIES TESTS
 * ============================================================================ */

int test_taylor_polynomial() {
    print_test("Taylor series of x² around x=0");

    // f(x) = x²
    // Taylor series around 0: x² (already a polynomial, should be exact)

    ASTNode *x = ast_create_variable("x");
    ASTNode *expr = ast_create_binary_op(OP_POWER, x, ast_create_number(2.0));

    ASTNode *taylor = ast_taylor_series(expr, "x", 0.0, 3);

    if (!taylor) {
        printf("  ERROR: Taylor series returned NULL\n");
        ast_free(expr);
        return 0;
    }

    printf("  Taylor series computed successfully\n");

    // Evaluate at x=2
    double x_val = 2.0;
    VarMapping mapping = {"x", 0};
    VarContext ctx = {.values = &x_val, .count = 1, .mappings = &mapping, .mapping_count = 1};

    double result = ast_evaluate(taylor, &ctx);
    double expected = 4.0;  // 2² = 4
    printf("  f(2) from Taylor: %.10f\n", result);
    printf("  f(2) exact:       %.10f\n", expected);
    ASSERT_CLOSE(result, expected, 1e-9);

    ast_free(expr);
    ast_free(taylor);
    return 1;
}

int test_taylor_exponential() {
    print_test("Taylor series of e^x around x=0");

    // e^x ≈ 1 + x + x²/2! + x³/3! + x⁴/4! + ...

    ASTNode *x = ast_create_variable("x");
    ASTNode *args[] = {x};
    ASTNode *expr = ast_create_function_call("EXP", args, 1);

    ASTNode *taylor = ast_taylor_series(expr, "x", 0.0, 10);
    printf("  Taylor series (order 10): 1 + x + x²/2 + x³/6 + x⁴/24 + ...\n");

    // Evaluate at x=1 (should approximate e ≈ 2.718281828)
    double x_val = 1.0;
    VarMapping mapping = {"x", 0};
    VarContext ctx = {.values = &x_val, .count = 1, .mappings = &mapping, .mapping_count = 1};

    double result = ast_evaluate(taylor, &ctx);
    double expected = exp(1.0);
    printf("  e^1 from Taylor: %.10f\n", result);
    printf("  e^1 exact:       %.10f\n", expected);
    ASSERT_CLOSE(result, expected, 1e-6);

    ast_free(expr);
    ast_free(taylor);
    return 1;
}

int test_taylor_sin() {
    print_test("Taylor series of sin(x) around x=0");

    // sin(x) ≈ x - x³/3! + x⁵/5! - x⁷/7! + ...

    ASTNode *x = ast_create_variable("x");
    ASTNode *args[] = {x};
    ASTNode *expr = ast_create_function_call("SIN", args, 1);

    ASTNode *taylor = ast_taylor_series(expr, "x", 0.0, 9);

    printf("  Taylor series (order 9): x - x³/6 + x⁵/120 - x⁷/5040 + ...\n");

    // Evaluate at x=0.5
    double x_val = 0.5;
    VarMapping mapping = {"x", 0};
    VarContext ctx = {.values = &x_val, .count = 1, .mappings = &mapping, .mapping_count = 1};

    double result = ast_evaluate(taylor, &ctx);
    double expected = sin(0.5);
    printf("  sin(0.5) from Taylor: %.10f\n", result);
    printf("  sin(0.5) exact:       %.10f\n", expected);
    ASSERT_CLOSE(result, expected, 1e-8);

    ast_free(expr);
    ast_free(taylor);
    return 1;
}

int test_taylor_cos_shifted() {
    print_test("Taylor series of cos(x) around x=π/2");

    // cos(x) around x=π/2 ≈ -(x-π/2) + (x-π/2)³/6 - ...

    ASTNode *x = ast_create_variable("x");
    ASTNode *args[] = {x};
    ASTNode *expr = ast_create_function_call("COS", args, 1);

    double center = M_PI / 2.0;
    ASTNode *taylor = ast_taylor_series(expr, "x", center, 7);

    printf("  Center: π/2 ≈ %.6f\n", center);

    // Evaluate at x=π/2 + 0.1
    double x_val = center + 0.1;
    VarMapping mapping = {"x", 0};
    VarContext ctx = {.values = &x_val, .count = 1, .mappings = &mapping, .mapping_count = 1};

    double result = ast_evaluate(taylor, &ctx);
    double expected = cos(x_val);
    printf("  cos(π/2+0.1) from Taylor: %.10f\n", result);
    printf("  cos(π/2+0.1) exact:       %.10f\n", expected);
    ASSERT_CLOSE(result, expected, 1e-7);

    ast_free(expr);
    ast_free(taylor);
    return 1;
}

/* ============================================================================
 * COMBINED TESTS
 * ============================================================================ */

int test_gradient_descent_step() {
    print_test("Gradient descent step on f(x,y) = x² + y²");

    // Minimum is at (0,0)
    // Start at (5,5), take one step with learning rate 0.1
    // New position: (x,y) - 0.1 * ∇f = (5,5) - 0.1*(10,10) = (4,4)

    ASTNode *x_sq = ast_create_binary_op(OP_POWER,
        ast_create_variable("x"), ast_create_number(2.0));
    ASTNode *y_sq = ast_create_binary_op(OP_POWER,
        ast_create_variable("y"), ast_create_number(2.0));
    ASTNode *expr = ast_create_binary_op(OP_ADD, x_sq, y_sq);

    const char *vars[] = {"x", "y"};
    Gradient grad = ast_gradient(expr, vars, 2);

    double pos[] = {5.0, 5.0};
    VarMapping mappings[] = {{"x", 0}, {"y", 1}};
    VarContext ctx = {.values = pos, .count = 2, .mappings = mappings, .mapping_count = 2};

    double *grad_vals = gradient_evaluate(&grad, &ctx);
    printf("  Initial position: (%.2f, %.2f)\n", pos[0], pos[1]);
    printf("  Gradient at position: [%.2f, %.2f]\n", grad_vals[0], grad_vals[1]);

    double learning_rate = 0.1;
    double new_x = pos[0] - learning_rate * grad_vals[0];
    double new_y = pos[1] - learning_rate * grad_vals[1];

    printf("  After gradient step: (%.2f, %.2f)\n", new_x, new_y);
    printf("  Expected: (4.0, 4.0)\n");

    ASSERT_CLOSE(new_x, 4.0, 1e-9);
    ASSERT_CLOSE(new_y, 4.0, 1e-9);

    free(grad_vals);
    gradient_free(&grad);
    ast_free(expr);
    return 1;
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================ */

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                  FLUXPARSER ADVANCED FEATURES                  ║\n");
    printf("║                      Test Suite v1.0                           ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");

    /* Numerical Integration Tests */
    print_section("NUMERICAL INTEGRATION");
    test_result(test_integrate_polynomial());
    test_result(test_integrate_sin());
    test_result(test_integrate_exponential());
    test_result(test_integrate_normal_distribution());

    /* Partial Derivatives & Gradient Tests */
    print_section("PARTIAL DERIVATIVES & GRADIENT");
    test_result(test_partial_derivative_simple());
    test_result(test_gradient_2d());
    test_result(test_gradient_3d());

    /* Taylor Series Tests */
    print_section("TAYLOR SERIES EXPANSION");
    test_result(test_taylor_polynomial());
    test_result(test_taylor_exponential());
    test_result(test_taylor_sin());
    test_result(test_taylor_cos_shifted());

    /* Combined Tests */
    print_section("COMBINED FEATURES");
    test_result(test_gradient_descent_step());

    /* Final Summary */
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                        TEST SUMMARY                            ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║  ✅ Passed: %-3d                                               ║\n", test_passed);
    printf("║  ❌ Failed: %-3d                                               ║\n", test_failed);
    printf("║  Total:    %-3d                                               ║\n", test_passed + test_failed);
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    return (test_failed == 0) ? 0 : 1;
}
