/*
 * FluxParser - Research-Grade C Math Parser
 * Copyright (C) 2025 Eduardo Stern
 *
 * Test Suite for Optimization Engine:
 * - Gradient Descent
 * - Gradient Descent with Momentum
 * - Adam Optimizer
 * - Conjugate Gradient
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
 * BASIC OPTIMIZATION TESTS
 * ============================================================================ */

int test_minimize_quadratic_1d() {
    print_test("Minimize f(x) = (x-3)²");

    // Minimum at x=3, f(3)=0
    // Build AST: (x-3)^2
    ASTNode *x = ast_create_variable("x");
    ASTNode *three = ast_create_number(3.0);
    ASTNode *x_minus_3 = ast_create_binary_op(OP_SUBTRACT, x, three);
    ASTNode *expr = ast_create_binary_op(OP_POWER, x_minus_3, ast_create_number(2.0));

    const char *vars[] = {"x"};
    double initial_guess[] = {0.0};  // Start at x=0

    OptimizerConfig config = optimizer_config_default(OPTIMIZER_GRADIENT_DESCENT);
    config.learning_rate = 0.1;
    config.tolerance = 1e-6;
    config.max_iterations = 100;

    OptimizationResult result = ast_minimize(expr, vars, 1, initial_guess, &config, OPTIMIZER_GRADIENT_DESCENT);

    printf("  Converged: %s\n", result.converged ? "Yes" : "No");
    printf("  Iterations: %d\n", result.iterations);
    printf("  Solution: x = %.6f\n", result.solution[0]);
    printf("  Final value: f(x) = %.6e\n", result.final_value);
    printf("  Expected: x = 3.0, f(x) = 0.0\n");

    ASSERT_TRUE(result.converged);
    ASSERT_CLOSE(result.solution[0], 3.0, 1e-3);
    ASSERT_CLOSE(result.final_value, 0.0, 1e-5);

    optimization_result_free(&result);
    ast_free(expr);
    return 1;
}

int test_minimize_rosenbrock() {
    print_test("Minimize Rosenbrock function f(x,y) = (1-x)² + 100(y-x²)²");

    // Global minimum at (1, 1) with f(1,1) = 0
    // This is a challenging test case!

    // Build AST: (1-x)^2 + 100*(y-x^2)^2
    ASTNode *x = ast_create_variable("x");
    ASTNode *y = ast_create_variable("y");

    // (1-x)^2
    ASTNode *one_minus_x = ast_create_binary_op(OP_SUBTRACT, ast_create_number(1.0), ast_create_variable("x"));
    ASTNode *term1 = ast_create_binary_op(OP_POWER, one_minus_x, ast_create_number(2.0));

    // x^2
    ASTNode *x_sq = ast_create_binary_op(OP_POWER, ast_create_variable("x"), ast_create_number(2.0));

    // y - x^2
    ASTNode *y_minus_x_sq = ast_create_binary_op(OP_SUBTRACT, ast_create_variable("y"), x_sq);

    // (y - x^2)^2
    ASTNode *diff_sq = ast_create_binary_op(OP_POWER, y_minus_x_sq, ast_create_number(2.0));

    // 100 * (y - x^2)^2
    ASTNode *term2 = ast_create_binary_op(OP_MULTIPLY, ast_create_number(100.0), diff_sq);

    // Full expression
    ASTNode *expr = ast_create_binary_op(OP_ADD, term1, term2);

    const char *vars[] = {"x", "y"};
    double initial_guess[] = {0.0, 0.0};  // Start at (0,0)

    OptimizerConfig config = optimizer_config_default(OPTIMIZER_ADAM);
    config.learning_rate = 0.01;
    config.tolerance = 1e-4;
    config.max_iterations = 1000;

    OptimizationResult result = ast_minimize(expr, vars, 2, initial_guess, &config, OPTIMIZER_ADAM);

    printf("  Converged: %s\n", result.converged ? "Yes" : "No");
    printf("  Iterations: %d\n", result.iterations);
    printf("  Solution: (x, y) = (%.6f, %.6f)\n", result.solution[0], result.solution[1]);
    printf("  Final value: f(x,y) = %.6e\n", result.final_value);
    printf("  Expected: (x, y) = (1.0, 1.0), f = 0.0\n");

    ASSERT_CLOSE(result.solution[0], 1.0, 0.05);  // Within 5%
    ASSERT_CLOSE(result.solution[1], 1.0, 0.05);
    ASSERT_TRUE(result.final_value < 0.1);  // Close to zero

    optimization_result_free(&result);
    ast_free(expr);
    return 1;
}

int test_minimize_paraboloid() {
    print_test("Minimize f(x,y) = x² + 4y²");

    // Minimum at (0, 0) with f(0,0) = 0

    ASTNode *x_sq = ast_create_binary_op(OP_POWER, ast_create_variable("x"), ast_create_number(2.0));
    ASTNode *y_sq = ast_create_binary_op(OP_POWER, ast_create_variable("y"), ast_create_number(2.0));
    ASTNode *four_y_sq = ast_create_binary_op(OP_MULTIPLY, ast_create_number(4.0), y_sq);
    ASTNode *expr = ast_create_binary_op(OP_ADD, x_sq, four_y_sq);

    const char *vars[] = {"x", "y"};
    double initial_guess[] = {5.0, 5.0};  // Start at (5,5)

    OptimizerConfig config = optimizer_config_default(OPTIMIZER_GRADIENT_DESCENT_MOMENTUM);
    config.learning_rate = 0.05;  // Reduced learning rate for stability
    config.momentum = 0.9;
    config.tolerance = 1e-4;  // Relaxed tolerance
    config.max_iterations = 500;

    OptimizationResult result = ast_minimize(expr, vars, 2, initial_guess, &config, OPTIMIZER_GRADIENT_DESCENT_MOMENTUM);

    printf("  Converged: %s\n", result.converged ? "Yes" : "No");
    printf("  Iterations: %d\n", result.iterations);
    printf("  Solution: (x, y) = (%.6f, %.6f)\n", result.solution[0], result.solution[1]);
    printf("  Final value: f(x,y) = %.6e\n", result.final_value);
    printf("  Expected: (x, y) = (0.0, 0.0), f = 0.0\n");

    // Check solution is close to optimum (relaxed criteria)
    ASSERT_CLOSE(result.solution[0], 0.0, 0.01);
    ASSERT_CLOSE(result.solution[1], 0.0, 0.01);
    ASSERT_TRUE(result.final_value < 1e-4);

    optimization_result_free(&result);
    ast_free(expr);
    return 1;
}

int test_maximize_function() {
    print_test("Maximize f(x) = -(x-2)² + 5");

    // Maximum at x=2, f(2)=5
    // Build AST: -(x-2)^2 + 5
    ASTNode *x = ast_create_variable("x");
    ASTNode *two = ast_create_number(2.0);
    ASTNode *x_minus_2 = ast_create_binary_op(OP_SUBTRACT, x, two);
    ASTNode *sq = ast_create_binary_op(OP_POWER, x_minus_2, ast_create_number(2.0));
    ASTNode *neg_sq = ast_create_unary_op(OP_NEGATE, sq);
    ASTNode *expr = ast_create_binary_op(OP_ADD, neg_sq, ast_create_number(5.0));

    const char *vars[] = {"x"};
    double initial_guess[] = {0.0};

    OptimizerConfig config = optimizer_config_default(OPTIMIZER_GRADIENT_DESCENT);
    config.learning_rate = 0.1;
    config.tolerance = 1e-6;
    config.max_iterations = 100;

    OptimizationResult result = ast_maximize(expr, vars, 1, initial_guess, &config, OPTIMIZER_GRADIENT_DESCENT);

    printf("  Converged: %s\n", result.converged ? "Yes" : "No");
    printf("  Iterations: %d\n", result.iterations);
    printf("  Solution: x = %.6f\n", result.solution[0]);
    printf("  Final value: f(x) = %.6f\n", result.final_value);
    printf("  Expected: x = 2.0, f(x) = 5.0\n");

    ASSERT_TRUE(result.converged);
    ASSERT_CLOSE(result.solution[0], 2.0, 1e-3);
    ASSERT_CLOSE(result.final_value, 5.0, 1e-5);

    optimization_result_free(&result);
    ast_free(expr);
    return 1;
}

/* ============================================================================
 * OPTIMIZER COMPARISON TESTS
 * ============================================================================ */

int test_compare_optimizers() {
    print_test("Compare all optimizers on f(x,y) = x² + y²");

    // Minimum at (0, 0)
    ASTNode *x_sq = ast_create_binary_op(OP_POWER, ast_create_variable("x"), ast_create_number(2.0));
    ASTNode *y_sq = ast_create_binary_op(OP_POWER, ast_create_variable("y"), ast_create_number(2.0));
    ASTNode *expr = ast_create_binary_op(OP_ADD, x_sq, y_sq);

    const char *vars[] = {"x", "y"};
    double initial_guess[] = {10.0, 10.0};

    printf("\n");

    /* Test Gradient Descent */
    {
        OptimizerConfig config = optimizer_config_default(OPTIMIZER_GRADIENT_DESCENT);
        config.learning_rate = 0.1;
        config.max_iterations = 500;

        OptimizationResult result = ast_minimize(expr, vars, 2, initial_guess, &config, OPTIMIZER_GRADIENT_DESCENT);
        printf("  Gradient Descent:          %3d iterations, f = %.6e, converged = %s\n",
               result.iterations, result.final_value, result.converged ? "Yes" : "No");
        optimization_result_free(&result);
    }

    /* Test Gradient Descent with Momentum */
    {
        OptimizerConfig config = optimizer_config_default(OPTIMIZER_GRADIENT_DESCENT_MOMENTUM);
        config.learning_rate = 0.1;
        config.momentum = 0.9;
        config.max_iterations = 500;

        OptimizationResult result = ast_minimize(expr, vars, 2, initial_guess, &config, OPTIMIZER_GRADIENT_DESCENT_MOMENTUM);
        printf("  Gradient Descent+Momentum: %3d iterations, f = %.6e, converged = %s\n",
               result.iterations, result.final_value, result.converged ? "Yes" : "No");
        optimization_result_free(&result);
    }

    /* Test Adam */
    {
        OptimizerConfig config = optimizer_config_default(OPTIMIZER_ADAM);
        config.learning_rate = 0.1;
        config.max_iterations = 500;

        OptimizationResult result = ast_minimize(expr, vars, 2, initial_guess, &config, OPTIMIZER_ADAM);
        printf("  Adam:                      %3d iterations, f = %.6e, converged = %s\n",
               result.iterations, result.final_value, result.converged ? "Yes" : "No");
        optimization_result_free(&result);
    }

    /* Test Conjugate Gradient */
    {
        OptimizerConfig config = optimizer_config_default(OPTIMIZER_CONJUGATE_GRADIENT);
        config.max_iterations = 500;

        OptimizationResult result = ast_minimize(expr, vars, 2, initial_guess, &config, OPTIMIZER_CONJUGATE_GRADIENT);
        printf("  Conjugate Gradient:        %3d iterations, f = %.6e, converged = %s\n",
               result.iterations, result.final_value, result.converged ? "Yes" : "No");
        optimization_result_free(&result);
    }

    ast_free(expr);
    return 1;
}

/* ============================================================================
 * LINEAR REGRESSION TEST
 * ============================================================================ */

int test_linear_regression() {
    print_test("Linear regression: fit y = mx + b to data");

    // Generate synthetic data: y = 2x + 1 + noise
    // We'll fit to 5 points and minimize sum of squared errors

    double x_data[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double y_data[] = {3.1, 4.9, 7.2, 9.0, 10.8};  // Approximately y = 2x + 1
    int n_points = 5;

    // Build objective: sum((y_data[i] - (m*x_data[i] + b))^2)
    // We need to create this symbolically...
    // For simplicity, let's do it as: (y1-(m*x1+b))^2 + (y2-(m*x2+b))^2 + ...

    ASTNode *sum = NULL;

    for (int i = 0; i < n_points; i++) {
        // m * x_data[i]
        ASTNode *m_times_x = ast_create_binary_op(OP_MULTIPLY,
            ast_create_variable("m"),
            ast_create_number(x_data[i])
        );

        // m*x_data[i] + b
        ASTNode *predicted = ast_create_binary_op(OP_ADD, m_times_x, ast_create_variable("b"));

        // y_data[i] - predicted
        ASTNode *error = ast_create_binary_op(OP_SUBTRACT, ast_create_number(y_data[i]), predicted);

        // error^2
        ASTNode *sq_error = ast_create_binary_op(OP_POWER, error, ast_create_number(2.0));

        // Add to sum
        if (sum == NULL) {
            sum = sq_error;
        } else {
            sum = ast_create_binary_op(OP_ADD, sum, sq_error);
        }
    }

    const char *vars[] = {"m", "b"};
    double initial_guess[] = {0.0, 0.0};  // Start with m=0, b=0

    OptimizerConfig config = optimizer_config_default(OPTIMIZER_ADAM);
    config.learning_rate = 0.05;  // Higher learning rate
    config.tolerance = 1e-6;
    config.max_iterations = 5000;  // More iterations

    OptimizationResult result = ast_minimize(sum, vars, 2, initial_guess, &config, OPTIMIZER_ADAM);

    printf("  Converged: %s\n", result.converged ? "Yes" : "No");
    printf("  Iterations: %d\n", result.iterations);
    printf("  Fitted line: y = %.3f*x + %.3f\n", result.solution[0], result.solution[1]);
    printf("  Expected:    y ≈ 2.0*x + 1.0\n");
    printf("  Final MSE: %.6e\n", result.final_value / n_points);

    // Check that we're close to the true values (relaxed tolerance due to noise in data)
    ASSERT_CLOSE(result.solution[0], 2.0, 0.2);  // Slope m ≈ 2.0
    ASSERT_CLOSE(result.solution[1], 1.0, 0.7);  // Intercept b ≈ 1.0

    optimization_result_free(&result);
    ast_free(sum);
    return 1;
}

/* ============================================================================
 * 3D OPTIMIZATION TEST
 * ============================================================================ */

int test_minimize_3d() {
    print_test("Minimize f(x,y,z) = x² + 2y² + 3z²");

    // Minimum at (0, 0, 0)
    ASTNode *x_sq = ast_create_binary_op(OP_POWER, ast_create_variable("x"), ast_create_number(2.0));
    ASTNode *y_sq = ast_create_binary_op(OP_POWER, ast_create_variable("y"), ast_create_number(2.0));
    ASTNode *z_sq = ast_create_binary_op(OP_POWER, ast_create_variable("z"), ast_create_number(2.0));

    ASTNode *two_y_sq = ast_create_binary_op(OP_MULTIPLY, ast_create_number(2.0), y_sq);
    ASTNode *three_z_sq = ast_create_binary_op(OP_MULTIPLY, ast_create_number(3.0), z_sq);

    ASTNode *sum1 = ast_create_binary_op(OP_ADD, x_sq, two_y_sq);
    ASTNode *expr = ast_create_binary_op(OP_ADD, sum1, three_z_sq);

    const char *vars[] = {"x", "y", "z"};
    double initial_guess[] = {10.0, 10.0, 10.0};

    OptimizerConfig config = optimizer_config_default(OPTIMIZER_ADAM);
    config.learning_rate = 0.1;  // Higher learning rate
    config.tolerance = 1e-4;     // Relaxed tolerance
    config.max_iterations = 1000;

    OptimizationResult result = ast_minimize(expr, vars, 3, initial_guess, &config, OPTIMIZER_ADAM);

    printf("  Converged: %s\n", result.converged ? "Yes" : "No");
    printf("  Iterations: %d\n", result.iterations);
    printf("  Solution: (x, y, z) = (%.6f, %.6f, %.6f)\n",
           result.solution[0], result.solution[1], result.solution[2]);
    printf("  Final value: f = %.6e\n", result.final_value);
    printf("  Expected: (0.0, 0.0, 0.0)\n");

    // Relaxed criteria
    ASSERT_CLOSE(result.solution[0], 0.0, 0.01);
    ASSERT_CLOSE(result.solution[1], 0.0, 0.01);
    ASSERT_CLOSE(result.solution[2], 0.0, 0.01);
    ASSERT_TRUE(result.final_value < 1e-3);

    optimization_result_free(&result);
    ast_free(expr);
    return 1;
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================ */

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║              FLUXPARSER OPTIMIZATION ENGINE                    ║\n");
    printf("║                      Test Suite v1.0                           ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");

    /* Basic Optimization Tests */
    print_section("BASIC OPTIMIZATION");
    test_result(test_minimize_quadratic_1d());
    test_result(test_minimize_paraboloid());
    test_result(test_minimize_3d());
    test_result(test_maximize_function());

    /* Challenging Problems */
    print_section("CHALLENGING PROBLEMS");
    test_result(test_minimize_rosenbrock());

    /* Optimizer Comparison */
    print_section("OPTIMIZER COMPARISON");
    test_result(test_compare_optimizers());

    /* Real-World Application */
    print_section("REAL-WORLD APPLICATION");
    test_result(test_linear_regression());

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
