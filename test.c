#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

#define MAX_INPUT 1024

void print_usage() {
    printf("FluxParser - Interactive Mode\n");
    printf("=====================================\n");
    printf("Commands:\n");
    printf("  help       - Show this help message\n");
    printf("  quit/exit  - Exit the program\n");
    printf("\nSupported operations:\n");
    printf("  Arithmetic: +, -, *, /, ^\n");
    printf("  Logical:    !, &&, ||\n");
    printf("  Grouping:   ( )\n");
    printf("\nConstants (case-insensitive):\n");
    printf("  PI                    - 3.14159...\n");
    printf("  E                     - 2.71828...\n");
    printf("\nMath functions (case-insensitive):\n");
    printf("  Zero arguments:\n");
    printf("    RANDOM(), RND()       - Random number [0,1)\n");
    printf("  One argument:\n");
    printf("    ABS(x)                - Absolute value\n");
    printf("    ROUND(x), FLOOR(x), CEIL(x), INT(x)\n");
    printf("    SQRT(x)               - Square root\n");
    printf("    SIN(x), COS(x), TAN(x)\n");
    printf("    ASIN(x), ACOS(x), ATAN(x)\n");
    printf("    LOG(x), LN(x)         - Natural logarithm\n");
    printf("    LOG10(x)              - Base-10 logarithm\n");
    printf("    EXP(x)                - e^x\n");
    printf("    SGN(x)                - Sign (-1, 0, or 1)\n");
    printf("  Two arguments:\n");
    printf("    MIN(x,y), MAX(x,y)\n");
    printf("    POW(x,y)              - x^y\n");
    printf("    MOD(x,y)              - Modulo\n");
    printf("    ATAN2(y,x)            - Two-argument arctangent\n");
    printf("\nOperator precedence (highest to lowest):\n");
    printf("  1. Functions, ! (unary NOT)\n");
    printf("  2. ^ (power, right-associative)\n");
    printf("  3. *, /\n");
    printf("  4. +, -\n");
    printf("  5. &&\n");
    printf("  6. ||\n");
    printf("\nExamples:\n");
    printf("  2 + 3 * 4           => 14.00\n");
    printf("  (2 + 3) * 4         => 20.00\n");
    printf("  2 ^ 3 ^ 2           => 512.00 (right-associative)\n");
    printf("  ABS(-5)             => 5.00\n");
    printf("  SQRT(16)            => 4.00\n");
    printf("  SIN(PI / 2)         => 1.00\n");
    printf("  MAX(10, 20)         => 20.00\n");
    printf("  ROUND(3.7)          => 4.00\n");
    printf("  2 * PI              => 6.28\n");
    printf("  EXP(1)              => 2.72 (approximately E)\n");
    printf("=====================================\n");
}

int main(int argc, char *argv[]) {
    char input[MAX_INPUT];

    /* Check if expression is provided as command line argument */
    if (argc > 1) {
        /* Check for debug flag */
        int expr_start = 1;
        if (argc > 2 && strcmp(argv[1], "-d") == 0) {
            expr_start = 2;
        }

        /* Concatenate all remaining arguments as the expression */
        input[0] = '\0';
        for (int i = expr_start; i < argc; i++) {
            strcat(input, argv[i]);
            if (i < argc - 1) strcat(input, " ");
        }

        float result = parse_expression(input);
        printf("Result: %.2f\n", result);
        return 0;
    }

    /* Interactive mode */
    print_usage();
    printf("\nEnter expressions (or 'help' for help):\n\n");

    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(input, MAX_INPUT, stdin)) {
            break;
        }

        /* Remove trailing newline */
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }

        /* Skip empty input */
        if (strlen(input) == 0) {
            continue;
        }

        /* Check for commands */
        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
            printf("Goodbye!\n");
            break;
        } else if (strcmp(input, "help") == 0) {
            printf("\n");
            print_usage();
            printf("\n");
            continue;
        }

        /* Parse and evaluate the expression */
        float result = parse_expression(input);
        printf("Result: %.2f\n", result);
        printf("\n");
    }

    return 0;
}
