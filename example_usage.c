/*
 * Example: How to use the parser with variables in your C program
 */

#include <stdio.h>
#include "parser.h"

int main() {
    printf("=== Simple Usage Example ===\n\n");

    /* Example 1: Using default single-letter variables (a-z) */
    printf("Example 1: Default single-letter variables\n");
    {
        float values[] = {10.0, 20.0, 5.0};  /* a=10, b=20, c=5 */

        VarContext ctx = {
            .values = values,
            .count = 3,
            .mappings = NULL,      /* NULL = use default a-z */
            .mapping_count = 0
        };

        float result = parse_expression_with_vars("(a + b) * c", &ctx);
        printf("  (a + b) * c with a=10, b=20, c=5 => %.2f\n", result);
    }

    /* Example 2: Custom variable names */
    printf("\nExample 2: Custom variable names\n");
    {
        float values[] = {100.0, 75.0, 0.15};

        VarMapping mappings[] = {
            {"PRICE", 0},
            {"QUANTITY", 1},
            {"TAXRATE", 2}
        };

        VarContext ctx = {
            .values = values,
            .count = 3,
            .mappings = mappings,
            .mapping_count = 3
        };

        float subtotal = parse_expression_with_vars("PRICE * QUANTITY", &ctx);
        printf("  Subtotal: %.2f\n", subtotal);

        float total = parse_expression_with_vars("PRICE * QUANTITY * (1 + TAXRATE)", &ctx);
        printf("  Total with tax: %.2f\n", total);
    }

    /* Example 3: Evaluating same expression with different values */
    printf("\nExample 3: Reusing expression with different values\n");
    {
        const char *distance_formula = "sqrt(a^2 + b^2)";

        VarContext ctx = {
            .values = NULL,  /* Will be set in loop */
            .count = 2,
            .mappings = NULL,
            .mapping_count = 0
        };

        float test_cases[][2] = {
            {3.0, 4.0},
            {5.0, 12.0},
            {8.0, 15.0}
        };

        for (int i = 0; i < 3; i++) {
            ctx.values = test_cases[i];
            float dist = parse_expression_with_vars(distance_formula, &ctx);
            printf("  Distance from (%.0f, %.0f): %.2f\n",
                   test_cases[i][0], test_cases[i][1], dist);
        }
    }

    /* Example 4: Integration with existing code - physics simulation */
    printf("\nExample 4: Physics calculation\n");
    {
        struct {
            float position;
            float velocity;
            float acceleration;
            float time;
        } physics = {0.0, 10.0, -9.81, 2.0};

        float values[] = {
            physics.position,
            physics.velocity,
            physics.acceleration,
            physics.time
        };

        VarMapping mappings[] = {
            {"X0", 0},   /* initial position */
            {"V0", 1},   /* initial velocity */
            {"A", 2},    /* acceleration */
            {"T", 3}     /* time */
        };

        VarContext ctx = {
            .values = values,
            .count = 4,
            .mappings = mappings,
            .mapping_count = 4
        };

        /* Position formula: x = x0 + v0*t + 0.5*a*t^2 */
        float position = parse_expression_with_vars("X0 + V0*T + 0.5*A*T^2", &ctx);
        printf("  Position after %.1fs: %.2f m\n", physics.time, position);

        /* Velocity formula: v = v0 + a*t */
        float velocity = parse_expression_with_vars("V0 + A*T", &ctx);
        printf("  Velocity after %.1fs: %.2f m/s\n", physics.time, velocity);
    }

    return 0;
}
