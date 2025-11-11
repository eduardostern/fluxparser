/*
 * FluxParser Example: Calculate Pi using Pythagorean Method
 *
 * Approximates Pi by inscribing polygons in a unit circle and
 * doubling the number of sides on each iteration.
 *
 * This demo runs TWO methods to compare:
 * 1. Direct expression evaluation: sqrt(2 - sqrt(4 - s^2))
 * 2. Numerical solver: solve x - sqrt(2 - sqrt(4 - s^2)) = 0
 *
 * Both methods should produce identical results, demonstrating
 * FluxParser's expression parser and numerical solver capabilities.
 */

#include "parser.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>

#define PI_ACTUAL 3.141592653589793

/* Global flag for graceful shutdown */
static volatile bool running = true;

void signal_handler(int signum) {
    (void)signum;
    running = false;
    printf("\n\nStopping...\n");
}

void run_method(const char* method_name, const char* expr_or_eq, bool use_solver) {
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║    Method %s: %-48s ║\n", use_solver ? "2" : "1", method_name);
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    printf("┌──────────────────┬──────────────┬──────────────────┬──────────────────────────┐\n");
    printf("│ Sides            │ Side Length  │ Pi Approximation │ Error                    │\n");
    printf("├──────────────────┼──────────────┼──────────────────┼──────────────────────────┤\n");

    /* Initial values: Equilateral triangle inscribed in unit circle */
    long sides = 3;
    double side_length = sqrt(3.0);  /* For triangle: side = sqrt(3) */

    /* Set up variable mappings for FluxParser */
    VarMapping mappings[] = {
        {"S", 0},  /* S = current side length */
        {"X", 1}   /* X = unknown for solver */
    };

    double values[2];
    VarContext ctx = {
        .values = values,
        .count = 2,
        .mappings = mappings,
        .mapping_count = 2
    };

    int iteration = 0;

    while (running) {
        /* Calculate Pi approximation: Pi = perimeter / 2 = (n * side) / 2 */
        double pi_approx = (sides * side_length) / 2.0;
        double error = fabs(pi_approx - PI_ACTUAL);
        double error_percent = (error / PI_ACTUAL) * 100.0;

        /* Print results with smart formatting */
        char side_str[13];
        if (side_length < 0.001) {
            snprintf(side_str, sizeof(side_str), "%12.3e", side_length);
        } else {
            snprintf(side_str, sizeof(side_str), "%12.9f", side_length);
        }

        char error_str[25];
        if (error_percent >= 0.01) {
            snprintf(error_str, sizeof(error_str), "%8.4f (%6.2f%%)", error, error_percent);
        } else {
            snprintf(error_str, sizeof(error_str), "%8.2e (%6.2f%%)", error, error_percent);
        }

        printf("│ %16ld │ %s │ %16.14f │ %-24s │\n",
               sides, side_str, pi_approx, error_str);
        fflush(stdout);

        /* Prevent loss of precision for very small side lengths */
        if (sides > 786432) {
            printf("└──────────────────┴──────────────┴──────────────────┴──────────────────────────┘\n\n");
            printf("✓ Reached optimal precision limit!\n");
            printf("  Final approximation: %.15f\n", pi_approx);
            printf("  Actual Pi:          %.15f\n", PI_ACTUAL);
            printf("  Error:              %.15e\n", error);
            break;
        }

        /* Calculate next side length using FluxParser */
        values[0] = side_length;  // Set S = current side length

        if (use_solver) {
            /* Method 2: Use numerical solver to find x where equation = 0 */
            /* Build AST for: x - sqrt(2 - sqrt(4 - s^2)) = 0 */

            /* Create s^2 */
            ASTNode *s_var = ast_create_variable("S");
            ASTNode *two_node = ast_create_number(2.0);
            ASTNode *s_squared = ast_create_binary_op(OP_POWER, ast_clone(s_var), ast_clone(two_node));

            /* Create 4 - s^2 */
            ASTNode *four_node = ast_create_number(4.0);
            ASTNode *four_minus_s2 = ast_create_binary_op(OP_SUBTRACT, four_node, s_squared);

            /* Create sqrt(4 - s^2) */
            ASTNode *args_inner[] = {four_minus_s2};
            ASTNode *sqrt_inner = ast_create_function_call("SQRT", args_inner, 1);

            /* Create 2 - sqrt(4 - s^2) */
            ASTNode *two_minus_sqrt = ast_create_binary_op(OP_SUBTRACT, two_node, sqrt_inner);

            /* Create sqrt(2 - sqrt(4 - s^2)) */
            ASTNode *args_outer[] = {two_minus_sqrt};
            ASTNode *sqrt_outer = ast_create_function_call("SQRT", args_outer, 1);

            /* Create x - sqrt(...) */
            ASTNode *x_var = ast_create_variable("X");
            ASTNode *equation = ast_create_binary_op(OP_SUBTRACT, x_var, sqrt_outer);

            /* Substitute the current value of S into the equation before solving */
            ASTNode *s_value_node = ast_create_number(side_length);
            ASTNode *equation_with_s = ast_substitute(equation, "S", s_value_node);
            ast_free(equation);
            ast_free(s_value_node);
            ast_free(s_var);

            /* Solve using Newton-Raphson: find x where equation = 0 */
            /* Use side_length/2 as initial guess to stay in the right neighborhood */
            double initial_guess = (side_length > 0.01) ? side_length / 2.0 : 0.001;
            NumericalSolveResult solve_result = ast_solve_numerical(
                equation_with_s, "X", initial_guess, 1e-10, 100);

            ast_free(equation_with_s);

            if (!solve_result.converged) {
                printf("\n\nSolver failed to converge: %s\n", solve_result.error_message);
                return;
            }

            side_length = solve_result.solution;
        } else {
            /* Method 1: Direct expression evaluation */
            ParseResult result = parse_expression_with_vars_safe(expr_or_eq, &ctx);

            if (result.has_error) {
                printf("\n\nError evaluating expression: %s\n", result.error.message);
                parser_print_error(expr_or_eq, &result);
                return;
            }

            side_length = result.value;
        }

        sides *= 2;
        iteration++;

        /* Add delay for readability */
        if (iteration < 20) {
            usleep(100000);  /* 100ms pause for first 20 iterations */
        }
    }

    if (!running) {
        /* User interrupted with Ctrl+C - print footer */
        printf("└──────────────────┴──────────────┴──────────────────┴──────────────────────────┘\n");
    }

    printf("\n");
}

int main() {
    /* Set up signal handler for Ctrl+C */
    signal(SIGINT, signal_handler);

    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║         FluxParser: Calculate Pi (Pythagorean Method)         ║\n");
    printf("║              Comparing Two Calculation Methods                ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    printf("Starting with triangle (3 sides), doubling each iteration\n");
    printf("Stopping at 786,432 sides (optimal double-precision limit)\n");
    printf("Press Ctrl+C to stop...\n\n");

    /* Method 1: Direct expression evaluation */
    run_method("Direct Expression Evaluation",
               "sqrt(2 - sqrt(4 - s^2))",
               false);

    printf("═══════════════════════════════════════════════════════════════\n\n");

    /* Reset running flag if user didn't interrupt */
    if (running) {
        /* Method 2: Numerical solver */
        run_method("Numerical Solver (Newton-Raphson)",
                   "x - sqrt(2 - sqrt(4 - s^2))",
                   true);
    }

    printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                     Calculation Complete!                     ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    printf("Both methods demonstrated:\n");
    printf("  1. Direct evaluation: parse and evaluate expression\n");
    printf("  2. Numerical solver:  solve equation for unknown variable\n\n");

    printf("Both should produce identical Pi approximations,\n");
    printf("demonstrating FluxParser's accuracy and versatility!\n\n");

    return 0;
}
