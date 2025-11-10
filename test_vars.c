#include <stdio.h>
#include <stdlib.h>
#include "parser.h"

void test_simple_variables() {
    printf("=== Test 1: Simple single-letter variables (a-z) ===\n");

    /* Array of values - will map to a, b, c, d, ... */
    double values[] = {5.0, 10.0, 3.0, 2.0};

    VarContext ctx = {
        .values = values,
        .count = 4,
        .mappings = NULL,  /* NULL = use default a-z mapping */
        .mapping_count = 0
    };

    printf("a=%.2f, b=%.2f, c=%.2f, d=%.2f\n", values[0], values[1], values[2], values[3]);

    double result;

    result = parse_expression_with_vars("a + b", &ctx);
    printf("a + b = %.2f\n", result);

    result = parse_expression_with_vars("a * b + c", &ctx);
    printf("a * b + c = %.2f\n", result);

    result = parse_expression_with_vars("(a + b) * c / d", &ctx);
    printf("(a + b) * c / d = %.2f\n", result);

    result = parse_expression_with_vars("sqrt(a^2 + b^2)", &ctx);
    printf("sqrt(a^2 + b^2) = %.2f\n", result);

    printf("\n");
}

void test_custom_names() {
    printf("=== Test 2: Custom variable names ===\n");

    /* Values array */
    double values[] = {100.0, 50.0, 25.0};

    /* Custom name mappings */
    VarMapping mappings[] = {
        {"WIDTH", 0},
        {"HEIGHT", 1},
        {"DEPTH", 2}
    };

    VarContext ctx = {
        .values = values,
        .count = 3,
        .mappings = mappings,
        .mapping_count = 3
    };

    printf("WIDTH=%.2f, HEIGHT=%.2f, DEPTH=%.2f\n", values[0], values[1], values[2]);

    double result;

    result = parse_expression_with_vars("WIDTH * HEIGHT", &ctx);
    printf("WIDTH * HEIGHT = %.2f (area)\n", result);

    result = parse_expression_with_vars("WIDTH * HEIGHT * DEPTH", &ctx);
    printf("WIDTH * HEIGHT * DEPTH = %.2f (volume)\n", result);

    result = parse_expression_with_vars("2 * (WIDTH + HEIGHT)", &ctx);
    printf("2 * (WIDTH + HEIGHT) = %.2f (perimeter)\n", result);

    printf("\n");
}

void test_physics_formulas() {
    printf("=== Test 3: Physics formulas ===\n");

    /* Physics constants and variables */
    double values[] = {9.81, 10.0, 5.0, 2.0};  /* g, m, v, t */

    VarMapping mappings[] = {
        {"G", 0},  /* gravity */
        {"M", 1},  /* mass */
        {"V", 2},  /* velocity */
        {"T", 3}   /* time */
    };

    VarContext ctx = {
        .values = values,
        .count = 4,
        .mappings = mappings,
        .mapping_count = 4
    };

    printf("G=%.2f (gravity), M=%.2f (mass), V=%.2f (velocity), T=%.2f (time)\n",
           values[0], values[1], values[2], values[3]);

    double result;

    /* Kinetic energy: 0.5 * m * v^2 */
    result = parse_expression_with_vars("0.5 * M * V^2", &ctx);
    printf("Kinetic energy (0.5 * M * V^2) = %.2f J\n", result);

    /* Distance fallen: 0.5 * g * t^2 */
    result = parse_expression_with_vars("0.5 * G * T^2", &ctx);
    printf("Distance fallen (0.5 * G * T^2) = %.2f m\n", result);

    /* Momentum: m * v */
    result = parse_expression_with_vars("M * V", &ctx);
    printf("Momentum (M * V) = %.2f kg*m/s\n", result);

    printf("\n");
}

void test_with_functions() {
    printf("=== Test 4: Variables with math functions ===\n");

    double values[] = {3.0, 4.0, -5.0};

    VarContext ctx = {
        .values = values,
        .count = 3,
        .mappings = NULL,
        .mapping_count = 0
    };

    printf("a=%.2f, b=%.2f, c=%.2f\n", values[0], values[1], values[2]);

    double result;

    result = parse_expression_with_vars("sqrt(a^2 + b^2)", &ctx);
    printf("sqrt(a^2 + b^2) = %.2f (Pythagorean)\n", result);

    result = parse_expression_with_vars("abs(c) + max(a, b)", &ctx);
    printf("abs(c) + max(a, b) = %.2f\n", result);

    result = parse_expression_with_vars("sin(a) + cos(b)", &ctx);
    printf("sin(a) + cos(b) = %.2f\n", result);

    result = parse_expression_with_vars("round(sqrt(abs(c)) * a)", &ctx);
    printf("round(sqrt(abs(c)) * a) = %.2f\n", result);

    printf("\n");
}

void test_debug_mode() {
    printf("=== Test 5: Debug mode with variables ===\n");

    double values[] = {2.0, 3.0};

    VarContext ctx = {
        .values = values,
        .count = 2,
        .mappings = NULL,
        .mapping_count = 0
    };

    set_debug_mode(true);
    double result = parse_expression_with_vars("a * a + b * b", &ctx);
    set_debug_mode(false);

    printf("Final result: %.2f\n\n", result);
}

int main(int argc, char *argv[]) {
    /* If command line arguments provided, evaluate that expression */
    if (argc >= 2) {
        /* Simple example: ./test_vars x+y 5 10 */
        if (argc >= 4) {
            const char *expr = argv[1];

            /* Parse variable values from command line */
            double values[26] = {0};  /* Support up to 26 variables */
            int count = 0;

            for (int i = 2; i < argc && count < 26; i++) {
                values[count++] = atof(argv[i]);
            }

            VarContext ctx = {
                .values = values,
                .count = count,
                .mappings = NULL,
                .mapping_count = 0
            };

            printf("Expression: %s\n", expr);
            printf("Variables: ");
            for (int i = 0; i < count; i++) {
                printf("%c=%.2f ", 'a' + i, values[i]);
            }
            printf("\n");

            double result = parse_expression_with_vars(expr, &ctx);
            printf("Result: %.2f\n", result);

            return 0;
        } else {
            printf("Usage: %s <expression> <value1> <value2> ...\n", argv[0]);
            printf("Example: %s \"x+y*z\" 5 10 3\n", argv[0]);
            printf("         (maps to a=5, b=10, c=3)\n\n");
            printf("Running demo tests instead...\n\n");
        }
    }

    /* Run all demo tests */
    test_simple_variables();
    test_custom_names();
    test_physics_formulas();
    test_with_functions();
    test_debug_mode();

    printf("All tests completed!\n");

    return 0;
}
