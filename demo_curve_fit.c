/*
 * FluxParser - Research-Grade C Math Parser
 * Copyright (C) 2025 Eduardo Stern
 *
 * DEMO: Polynomial Curve Fitting using Optimization Engine
 *
 * This demo fits a polynomial to noisy data points using gradient descent.
 * It demonstrates the power of FluxParser's optimization capabilities.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "parser.h"
#include "ast.h"

/* Generate synthetic data from a known polynomial with noise */
void generate_data(double *x_data, double *y_data, int n_points) {
    // True function: y = 0.5x³ - 2x² + x + 3 + noise
    srand(time(NULL));

    for (int i = 0; i < n_points; i++) {
        x_data[i] = -2.0 + 4.0 * i / (n_points - 1.0);  // x from -2 to 2

        double x = x_data[i];
        double y_true = 0.5*x*x*x - 2.0*x*x + x + 3.0;
        double noise = ((double)rand() / RAND_MAX - 0.5) * 0.5;  // ±0.25 noise

        y_data[i] = y_true + noise;
    }
}

/* Print data points in a simple ASCII plot */
void plot_data(double *x_data, double *y_data, int n_points, const char *title) {
    printf("\n%s\n", title);
    printf("─────────────────────────────────────────────────────────\n");

    // Find y range
    double y_min = y_data[0], y_max = y_data[0];
    for (int i = 1; i < n_points; i++) {
        if (y_data[i] < y_min) y_min = y_data[i];
        if (y_data[i] > y_max) y_max = y_data[i];
    }

    // Simple table output
    for (int i = 0; i < n_points; i++) {
        printf("  x=%6.2f  y=%7.3f  ", x_data[i], y_data[i]);

        // ASCII bar
        int bar_len = (int)((y_data[i] - y_min) / (y_max - y_min) * 40);
        for (int j = 0; j < bar_len; j++) printf("█");
        printf("\n");
    }
    printf("─────────────────────────────────────────────────────────\n");
}

/* Fit a polynomial to data using FluxParser optimization */
OptimizationResult fit_polynomial(
    double *x_data,
    double *y_data,
    int n_points,
    int degree,
    OptimizerType optimizer
) {
    // Build objective function: sum of squared errors
    // SSE = Σ(y_data[i] - (a₀ + a₁x + a₂x² + ... + aₙxⁿ))²

    ASTNode *sse = NULL;

    for (int i = 0; i < n_points; i++) {
        // Build polynomial: a0 + a1*x_i + a2*x_i^2 + ... + an*x_i^n
        ASTNode *poly = NULL;

        for (int deg = 0; deg <= degree; deg++) {
            ASTNode *term;

            if (deg == 0) {
                // Constant term: a0
                term = ast_create_variable("a0");
            } else if (deg == 1) {
                // Linear term: a1 * x_i
                char var_name[4];
                snprintf(var_name, sizeof(var_name), "a%d", deg);
                term = ast_create_binary_op(OP_MULTIPLY,
                    ast_create_variable(var_name),
                    ast_create_number(x_data[i])
                );
            } else {
                // Higher order: a_deg * x_i^deg
                char var_name[4];
                snprintf(var_name, sizeof(var_name), "a%d", deg);

                ASTNode *x_power = ast_create_binary_op(OP_POWER,
                    ast_create_number(x_data[i]),
                    ast_create_number((double)deg)
                );

                term = ast_create_binary_op(OP_MULTIPLY,
                    ast_create_variable(var_name),
                    x_power
                );
            }

            // Add to polynomial
            if (poly == NULL) {
                poly = term;
            } else {
                poly = ast_create_binary_op(OP_ADD, poly, term);
            }
        }

        // Error: y_data[i] - poly
        ASTNode *error = ast_create_binary_op(OP_SUBTRACT,
            ast_create_number(y_data[i]),
            poly
        );

        // Squared error
        ASTNode *sq_error = ast_create_binary_op(OP_POWER,
            error,
            ast_create_number(2.0)
        );

        // Add to sum
        if (sse == NULL) {
            sse = sq_error;
        } else {
            sse = ast_create_binary_op(OP_ADD, sse, sq_error);
        }
    }

    // Setup variables: a0, a1, a2, ..., a_degree
    const char **var_names = malloc(sizeof(char*) * (degree + 1));
    double *initial_guess = calloc(degree + 1, sizeof(double));

    for (int i = 0; i <= degree; i++) {
        char *name = malloc(4);
        snprintf(name, 4, "a%d", i);
        var_names[i] = name;
        initial_guess[i] = 0.0;  // Start with all coefficients = 0
    }

    // Configure optimizer
    OptimizerConfig config = optimizer_config_default(optimizer);

    if (optimizer == OPTIMIZER_ADAM) {
        config.learning_rate = 0.05;
        config.max_iterations = 10000;
        config.tolerance = 1e-6;
    } else if (optimizer == OPTIMIZER_GRADIENT_DESCENT) {
        config.learning_rate = 0.001;
        config.max_iterations = 20000;
        config.tolerance = 1e-6;
    } else if (optimizer == OPTIMIZER_GRADIENT_DESCENT_MOMENTUM) {
        config.learning_rate = 0.01;
        config.momentum = 0.9;
        config.max_iterations = 10000;
        config.tolerance = 1e-6;
    }

    // Run optimization
    OptimizationResult result = ast_minimize(sse, var_names, degree + 1, initial_guess, &config, optimizer);

    // Cleanup
    ast_free(sse);
    for (int i = 0; i <= degree; i++) {
        free((void*)var_names[i]);
    }
    free(var_names);
    free(initial_guess);

    return result;
}

/* Evaluate fitted polynomial at given x */
double eval_poly(double x, double *coeffs, int degree) {
    double y = 0.0;
    double x_power = 1.0;

    for (int i = 0; i <= degree; i++) {
        y += coeffs[i] * x_power;
        x_power *= x;
    }

    return y;
}

/* Compute R² score */
double compute_r_squared(double *x_data, double *y_data, int n_points, double *coeffs, int degree) {
    // Compute mean of y
    double y_mean = 0.0;
    for (int i = 0; i < n_points; i++) {
        y_mean += y_data[i];
    }
    y_mean /= n_points;

    // Compute SS_tot and SS_res
    double ss_tot = 0.0;
    double ss_res = 0.0;

    for (int i = 0; i < n_points; i++) {
        double y_pred = eval_poly(x_data[i], coeffs, degree);
        ss_tot += (y_data[i] - y_mean) * (y_data[i] - y_mean);
        ss_res += (y_data[i] - y_pred) * (y_data[i] - y_pred);
    }

    return 1.0 - (ss_res / ss_tot);
}

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║        FLUXPARSER POLYNOMIAL CURVE FITTING DEMO                ║\n");
    printf("║                                                                ║\n");
    printf("║  Demonstrates gradient-based optimization to fit polynomials  ║\n");
    printf("║  to noisy data points.                                        ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");

    // Generate synthetic data
    int n_points = 20;
    double *x_data = malloc(sizeof(double) * n_points);
    double *y_data = malloc(sizeof(double) * n_points);

    generate_data(x_data, y_data, n_points);

    // Display data
    plot_data(x_data, y_data, n_points, "📊 NOISY DATA POINTS");

    printf("\n");
    printf("True function: y = 0.5x³ - 2x² + x + 3 (+ noise)\n");
    printf("\n");

    // Fit different polynomial degrees
    int degrees[] = {1, 2, 3};
    const char *degree_names[] = {"Linear", "Quadratic", "Cubic"};

    for (int d = 0; d < 3; d++) {
        int degree = degrees[d];

        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("  FITTING %s POLYNOMIAL (degree %d)\n", degree_names[d], degree);
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

        // Fit using Adam optimizer
        OptimizationResult result = fit_polynomial(x_data, y_data, n_points, degree, OPTIMIZER_ADAM);

        if (result.converged || result.iterations >= 100) {
            printf("  ✓ Converged: %s\n", result.converged ? "Yes" : "No (stopped early)");
            printf("  ✓ Iterations: %d\n", result.iterations);
            printf("  ✓ Final MSE: %.6f\n", result.final_value / n_points);

            // Print fitted polynomial
            printf("\n  Fitted polynomial:\n  y = ");
            for (int i = degree; i >= 0; i--) {
                if (i == degree) {
                    printf("%.4f", result.solution[i]);
                } else {
                    printf(" %c %.4f", result.solution[i] >= 0 ? '+' : '-', fabs(result.solution[i]));
                }

                if (i > 1) printf("x^%d", i);
                else if (i == 1) printf("x");
            }
            printf("\n\n");

            // Compute R²
            double r_squared = compute_r_squared(x_data, y_data, n_points, result.solution, degree);
            printf("  R² score: %.4f ", r_squared);
            if (r_squared > 0.95) printf("(Excellent fit! ⭐)\n");
            else if (r_squared > 0.85) printf("(Good fit)\n");
            else printf("(Poor fit)\n");

        } else {
            printf("  ✗ Optimization failed: %s\n", result.error_message);
        }

        optimization_result_free(&result);
        printf("\n");
    }

    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                     DEMO COMPLETE!                             ║\n");
    printf("║                                                                ║\n");
    printf("║  The cubic fit should recover the original function almost    ║\n");
    printf("║  perfectly (coefficients: 0.5, -2.0, 1.0, 3.0)                ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    // Cleanup
    free(x_data);
    free(y_data);

    return 0;
}
