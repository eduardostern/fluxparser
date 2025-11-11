/*
 * FluxParser Example: Calculate Pi using Pythagorean Method
 *
 * Approximates Pi by inscribing polygons in a unit circle and
 * doubling the number of sides on each iteration.
 *
 * Method:
 * 1. Start with hexagon (6 sides, side length = 1.0)
 * 2. Calculate perimeter = n * side_length
 * 3. Approximate Pi = perimeter / 2
 * 4. Double sides using: side_new = sqrt(2 - sqrt(4 - side_old^2))
 * 5. Repeat until interrupted
 */

#include "parser.h"
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

int main() {
    /* Set up signal handler for Ctrl+C */
    signal(SIGINT, signal_handler);

    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║        FluxParser: Calculate Pi (Pythagorean Method)         ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    printf("Method: Inscribed polygons using Pythagorean theorem\n");
    printf("Formula: side_new = sqrt(2 - sqrt(4 - side_old^2))\n");
    printf("Starting with triangle (3 sides)\n");
    printf("Press Ctrl+C to stop...\n\n");

    printf("┌──────────┬─────────────┬──────────────────┬──────────────────┐\n");
    printf("│ Sides    │ Side Length │ Pi Approximation │ Error            │\n");
    printf("├──────────┼─────────────┼──────────────────┼──────────────────┤\n");

    /* Initial values: Equilateral triangle inscribed in unit circle */
    long sides = 3;
    double side_length = sqrt(3.0);  /* For triangle: side = sqrt(3) */

    /* Set up variable mappings for FluxParser */
    /* Note: Parser converts identifiers to uppercase, so use "S" not "s" */
    VarMapping mappings[] = {
        {"S", 0}  /* S = current side length */
    };

    double values[1];
    VarContext ctx = {
        .values = values,
        .count = 1,
        .mappings = mappings,
        .mapping_count = 1
    };

    /* Expression to calculate next side length using Pythagorean theorem */
    const char *next_side_expr = "sqrt(2 - sqrt(4 - s^2))";

    int iteration = 0;

    while (running) {
        /* Calculate Pi approximation: Pi = perimeter / 2 = (n * side) / 2 */
        double pi_approx = (sides * side_length) / 2.0;
        double error = fabs(pi_approx - PI_ACTUAL);
        double error_percent = (error / PI_ACTUAL) * 100.0;

        /* Print results */
        printf("│ %8ld │ %11.9f │ %16.14f │ %.6e (%.4f%%) │\n",
               sides, side_length, pi_approx, error, error_percent);
        fflush(stdout);

        /* Check convergence (when error is extremely small) */
        if (error < 1e-14) {
            printf("└──────────┴─────────────┴──────────────────┴──────────────────┘\n\n");
            printf("✓ Converged to machine precision!\n");
            printf("  Final approximation: %.15f\n", pi_approx);
            printf("  Actual Pi:          %.15f\n", PI_ACTUAL);
            printf("  Error:              %.15e\n", error);
            break;
        }

        /* Prevent overflow for very large polygon counts */
        if (sides > 1e15) {
            printf("└──────────┴─────────────┴──────────────────┴──────────────────┘\n\n");
            printf("⚠ Reached numerical limits (sides > 10^15)\n");
            printf("  Best approximation: %.15f\n", pi_approx);
            break;
        }

        /* Calculate next side length using FluxParser */
        values[0] = side_length;
        ParseResult result = parse_expression_with_vars_safe(next_side_expr, &ctx);

        if (result.has_error) {
            printf("\n\nError calculating next iteration: %s\n", result.error.message);
            parser_print_error(next_side_expr, &result);
            return 1;
        }

        side_length = result.value;
        sides *= 2;
        iteration++;

        /* Add delay for readability (can be removed for max speed) */
        if (iteration < 20) {
            usleep(200000);  /* 200ms pause for first 20 iterations */
        } else if (iteration < 40) {
            usleep(50000);   /* 50ms pause for iterations 20-40 */
        }
        /* No delay after iteration 40 */
    }

    if (running) {
        /* If we didn't break early, print footer */
        printf("└──────────┴─────────────┴──────────────────┴──────────────────┘\n");
    }

    printf("\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("FluxParser successfully calculated %d iterations!\n", iteration);
    printf("═══════════════════════════════════════════════════════════════\n");

    /* Show what expression was used */
    printf("\nExpression used: %s\n", next_side_expr);
    printf("Where 's' is the current side length of the inscribed polygon.\n");

    return 0;
}
