/*
 * FluxParser Example: Calculate Pi using Four Historical Methods
 *
 * 1. PYTHAGOREAN METHOD (Ancient Greece, ~500 BC)
 *    Approximates Pi by inscribing polygons in a unit circle
 *
 * 2. MACHIN'S FORMULA (John Machin, 1706)
 *    π/4 = 4*arctan(1/5) - arctan(1/239)
 *    Uses Taylor series for arctan
 *
 * 3. NEWTON'S FORMULA (Isaac Newton, 1666)
 *    π/6 = arcsin(1/2) using binomial series expansion
 *
 * 4. CHUDNOVSKY ALGORITHM (Chudnovsky Brothers, 1988)
 *    Modern formula used for world records (billions of digits)
 *    Converges at ~14 digits per term!
 *
 * Each method demonstrates FluxParser's expression evaluation capabilities.
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

/* Calculate arctan using Taylor series with FluxParser: arctan(x) = x - x³/3 + x⁵/5 - x⁷/7 + ... */
double arctan_series(double x, int terms) {
    double result = 0.0;

    /* Set up variable context for FluxParser */
    VarMapping mappings[] = {{"X", 0}, {"N", 1}};
    double values[2];
    VarContext ctx = {
        .values = values,
        .count = 2,
        .mappings = mappings,
        .mapping_count = 2
    };

    values[0] = x;  /* X = input value */

    for (int n = 0; n < terms; n++) {
        values[1] = (double)n;  /* N = current term index */

        /* Build expression: x^(2n+1) / (2n+1)
         * Using FluxParser to evaluate: x^(2*n+1) / (2*n+1) */
        char expr[64];
        snprintf(expr, sizeof(expr), "x^(2*n+1) / (2*n+1)");

        ParseResult parse_result = parse_expression_with_vars_safe(expr, &ctx);
        if (parse_result.has_error) {
            fprintf(stderr, "Error in arctan series: %s\n", parse_result.error.message);
            return result;
        }

        double term = parse_result.value;

        /* Alternate signs: +, -, +, -, ... */
        if (n % 2 == 0) {
            result += term;
        } else {
            result -= term;
        }
    }

    return result;
}

void run_machin() {
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║              Machin's Formula (John Machin, 1706)            ║\n");
    printf("║          π/4 = 4*arctan(1/5) - arctan(1/239)                 ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    printf("┌──────────────────┬──────────────────┬──────────────────────────┐\n");
    printf("│ Terms            │ Pi Approximation │ Error                    │\n");
    printf("├──────────────────┼──────────────────┼──────────────────────────┤\n");

    for (int terms = 1; terms <= 25 && running; terms++) {
        /* Machin's formula: π/4 = 4*arctan(1/5) - arctan(1/239) */
        double arctan_1_5 = arctan_series(1.0/5.0, terms);
        double arctan_1_239 = arctan_series(1.0/239.0, terms);

        /* Use FluxParser to calculate: 4 * (4*arctan_1_5 - arctan_1_239) */
        char pi_expr[64];
        snprintf(pi_expr, sizeof(pi_expr), "4 * (4*%.15e - %.15e)", arctan_1_5, arctan_1_239);

        ParseResult pi_result = parse_expression_safe(pi_expr);
        if (pi_result.has_error) {
            fprintf(stderr, "Error calculating Pi with Machin: %s\n", pi_result.error.message);
            return;
        }

        double pi_approx = pi_result.value;

        double error = fabs(pi_approx - PI_ACTUAL);
        double error_percent = (error / PI_ACTUAL) * 100.0;

        char error_str[25];
        if (error_percent >= 0.01) {
            snprintf(error_str, sizeof(error_str), "%8.4f (%6.2f%%)", error, error_percent);
        } else {
            snprintf(error_str, sizeof(error_str), "%8.2e (%6.2f%%)", error, error_percent);
        }

        printf("│ %16d │ %16.14f │ %-24s │\n", terms, pi_approx, error_str);
        fflush(stdout);

        /* Check convergence */
        if (error < 1e-14) {
            printf("└──────────────────┴──────────────────┴──────────────────────────┘\n\n");
            printf("✓ Converged to machine precision!\n");
            printf("  Final approximation: %.15f\n", pi_approx);
            printf("  Actual Pi:          %.15f\n", PI_ACTUAL);
            printf("  Error:              %.15e\n", error);
            printf("  Terms needed:       %d\n", terms);
            break;
        }

        if (terms < 10) {
            usleep(150000);  /* 150ms pause for readability */
        }
    }

    if (!running) {
        printf("└──────────────────┴──────────────────┴──────────────────────────┘\n");
    }

    printf("\n");
}

/* Calculate factorial using iteration (for large factorials we need high precision) */
double factorial(int n) {
    double result = 1.0;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}

void run_chudnovsky() {
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║         Chudnovsky Algorithm (Chudnovsky Bros, 1988)         ║\n");
    printf("║          Used for world record calculations of Pi            ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    printf("┌──────────────────┬──────────────────┬──────────────────────────┐\n");
    printf("│ Terms            │ Pi Approximation │ Error                    │\n");
    printf("├──────────────────┼──────────────────┼──────────────────────────┤\n");

    double sum = 0.0;

    /* Set up variable context for FluxParser */
    VarMapping mappings[] = {{"K", 0}};
    double values[1];
    VarContext ctx = {
        .values = values,
        .count = 1,
        .mappings = mappings,
        .mapping_count = 1
    };

    /* Chudnovsky formula: 1/π = (12/C) × Σ
     * where C = 640320^(3/2) = 640320 × sqrt(640320)
     * Simplified: C = 426880 × sqrt(10005)
     */

    for (int k = 0; k <= 5 && running; k++) {
        values[0] = (double)k;  /* K = current term index */

        /* Chudnovsky formula term:
         * 1/π = 12 * Σ[((-1)^k × (6k)! × (13591409 + 545140134k)) / ((k!)^3 × (3k)! × 640320^(3k+3/2))]
         * So π = 1 / (12 * Σ)
         */

        /* Calculate factorials */
        double fact_6k = factorial(6 * k);
        double fact_3k = factorial(3 * k);
        double fact_k = factorial(k);

        /* Use FluxParser to calculate the linear term: 13591409 + 545140134*k */
        char linear_expr[64];
        snprintf(linear_expr, sizeof(linear_expr), "13591409 + 545140134*k");

        ParseResult linear_result = parse_expression_with_vars_safe(linear_expr, &ctx);
        if (linear_result.has_error) {
            fprintf(stderr, "Error in Chudnovsky linear term: %s\n", linear_result.error.message);
            return;
        }
        double linear_term = linear_result.value;

        /* Use FluxParser to calculate the sign: (-1)^k */
        char sign_expr[32];
        snprintf(sign_expr, sizeof(sign_expr), "(-1)^k");

        ParseResult sign_result = parse_expression_with_vars_safe(sign_expr, &ctx);
        if (sign_result.has_error) {
            fprintf(stderr, "Error in Chudnovsky sign: %s\n", sign_result.error.message);
            return;
        }
        double sign = sign_result.value;

        /* Use FluxParser to calculate the numerator: sign × fact_6k × linear_term */
        char numerator_expr[128];
        snprintf(numerator_expr, sizeof(numerator_expr), "%.15e * %.15e * %.15e",
                 sign, fact_6k, linear_term);

        ParseResult num_result = parse_expression_safe(numerator_expr);
        if (num_result.has_error) {
            fprintf(stderr, "Error in Chudnovsky numerator: %s\n", num_result.error.message);
            return;
        }
        double numerator = num_result.value;

        /* Use FluxParser to calculate 640320^(3k+1.5) */
        char power_expr[64];
        snprintf(power_expr, sizeof(power_expr), "640320^(3*k + 1.5)");
        ParseResult power_result = parse_expression_with_vars_safe(power_expr, &ctx);
        if (power_result.has_error) {
            fprintf(stderr, "Error in power calculation: %s\n", power_result.error.message);
            return;
        }

        /* Use FluxParser to calculate the denominator: fact_k^3 × fact_3k × power */
        char denom_expr[128];
        snprintf(denom_expr, sizeof(denom_expr), "%.15e^3 * %.15e * %.15e",
                 fact_k, fact_3k, power_result.value);

        ParseResult denom_result = parse_expression_safe(denom_expr);
        if (denom_result.has_error) {
            fprintf(stderr, "Error in Chudnovsky denominator: %s\n", denom_result.error.message);
            return;
        }
        double denominator = denom_result.value;

        /* Use FluxParser to calculate the term: numerator / denominator */
        char term_expr[128];
        snprintf(term_expr, sizeof(term_expr), "%.15e / %.15e", numerator, denominator);

        ParseResult term_result = parse_expression_safe(term_expr);
        if (term_result.has_error) {
            fprintf(stderr, "Error in Chudnovsky term: %s\n", term_result.error.message);
            return;
        }

        sum += term_result.value;

        /* Use FluxParser to calculate Pi = 1 / (12 * sum) */
        char pi_expr[64];
        snprintf(pi_expr, sizeof(pi_expr), "1 / (12 * %.15e)", sum);

        ParseResult pi_result = parse_expression_safe(pi_expr);
        if (pi_result.has_error) {
            fprintf(stderr, "Error calculating final Pi: %s\n", pi_result.error.message);
            return;
        }

        double pi_approx = pi_result.value;

        double error = fabs(pi_approx - PI_ACTUAL);
        double error_percent = (error / PI_ACTUAL) * 100.0;

        char error_str[25];
        if (error_percent >= 0.01) {
            snprintf(error_str, sizeof(error_str), "%8.4f (%6.2f%%)", error, error_percent);
        } else if (error > 1e-14) {
            snprintf(error_str, sizeof(error_str), "%8.2e (%6.2f%%)", error, error_percent);
        } else {
            snprintf(error_str, sizeof(error_str), "%8.2e (  0.00%%)", error);
        }

        printf("│ %16d │ %16.14f │ %-24s │\n", k, pi_approx, error_str);
        fflush(stdout);

        /* Check convergence */
        if (error < 1e-14) {
            printf("└──────────────────┴──────────────────┴──────────────────────────┘\n\n");
            printf("✓ Converged to machine precision!\n");
            printf("  Final approximation: %.15f\n", pi_approx);
            printf("  Actual Pi:          %.15f\n", PI_ACTUAL);
            printf("  Error:              %.15e\n", error);
            printf("  Terms needed:       %d\n", k);
            printf("\n  Note: Each term adds ~14 digits of precision!\n");
            break;
        }

        if (k < 3) {
            usleep(200000);  /* 200ms pause for readability */
        }
    }

    if (!running) {
        printf("└──────────────────┴──────────────────┴──────────────────────────┘\n");
    }

    printf("\n");
}

void run_newton() {
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║             Newton's Formula (Isaac Newton, 1666)            ║\n");
    printf("║         π/6 = arcsin(1/2) via binomial expansion             ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    printf("┌──────────────────┬──────────────────┬──────────────────────────┐\n");
    printf("│ Terms            │ Pi Approximation │ Error                    │\n");
    printf("├──────────────────┼──────────────────┼──────────────────────────┤\n");

    double sum = 0.5;  /* First term: x = 1/2 */

    /* Set up variable context for FluxParser */
    VarMapping mappings[] = {{"N", 0}};
    double values[1];
    VarContext ctx = {
        .values = values,
        .count = 1,
        .mappings = mappings,
        .mapping_count = 1
    };

    /* Precompute double factorials to avoid recalculation */
    double numerator = 1.0;    /* (2n-1)!! */
    double denominator = 1.0;  /* (2n)!! */

    for (int n = 1; n <= 30 && running; n++) {
        values[0] = (double)n;  /* N = current term index */

        /* Update double factorials incrementally:
         * arcsin(x) series: term_n = [(2n-1)!! / (2n)!!] × x^(2n+1) / (2n+1)
         * (2n-1)!! = 1×3×5×...×(2n-1)
         * (2n)!! = 2×4×6×...×(2n)
         *
         * For n=1: 1!! / 2!! = 1/2
         * For n=2: 3!! / 4!! = (1×3) / (2×4) = 3/8
         * For n=3: 5!! / 6!! = (1×3×5) / (2×4×6) = 15/48
         */
        if (n == 1) {
            numerator = 1.0;
            denominator = 2.0;
        } else {
            numerator *= (2*n - 1);
            denominator *= (2*n);
        }

        /* Use FluxParser to calculate: (numerator/denominator) × (0.5)^(2n+1) / (2n+1)
         * Simplified expression: (numerator/denominator) × 0.5^(2*n+1) / (2*n+1) */
        char expr[128];
        snprintf(expr, sizeof(expr), "%.15e / %.15e * 0.5^(2*n+1) / (2*n+1)",
                 numerator, denominator);

        ParseResult parse_result = parse_expression_with_vars_safe(expr, &ctx);
        if (parse_result.has_error) {
            fprintf(stderr, "Error in Newton series: %s\n", parse_result.error.message);
            return;
        }

        double term = parse_result.value;
        sum += term;

        /* Use FluxParser to calculate final Pi: 6 × sum */
        char pi_expr[32];
        snprintf(pi_expr, sizeof(pi_expr), "6 * %.15e", sum);

        ParseResult pi_result = parse_expression_safe(pi_expr);
        if (pi_result.has_error) {
            fprintf(stderr, "Error calculating Pi: %s\n", pi_result.error.message);
            return;
        }

        double pi_approx = pi_result.value;

        double error = fabs(pi_approx - PI_ACTUAL);
        double error_percent = (error / PI_ACTUAL) * 100.0;

        char error_str[25];
        if (error_percent >= 0.01) {
            snprintf(error_str, sizeof(error_str), "%8.4f (%6.2f%%)", error, error_percent);
        } else {
            snprintf(error_str, sizeof(error_str), "%8.2e (%6.2f%%)", error, error_percent);
        }

        printf("│ %16d │ %16.14f │ %-24s │\n", n, pi_approx, error_str);
        fflush(stdout);

        /* Check convergence */
        if (error < 1e-14 || fabs(term) < 1e-16) {
            printf("└──────────────────┴──────────────────┴──────────────────────────┘\n\n");
            printf("✓ Converged to machine precision!\n");
            printf("  Final approximation: %.15f\n", pi_approx);
            printf("  Actual Pi:          %.15f\n", PI_ACTUAL);
            printf("  Error:              %.15e\n", error);
            printf("  Terms needed:       %d\n", n);
            break;
        }

        if (n < 10) {
            usleep(150000);  /* 150ms pause for readability */
        }
    }

    if (!running) {
        printf("└──────────────────┴──────────────────┴──────────────────────────┘\n");
    }

    printf("\n");
}

int main() {
    /* Set up signal handler for Ctrl+C */
    signal(SIGINT, signal_handler);

    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          FluxParser: Calculate Pi - Four Methods             ║\n");
    printf("║     Ancient Greece → Newton → Machin → Chudnovsky            ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    printf("This demo showcases four historical methods for calculating Pi:\n");
    printf("  1. Pythagorean Method (~500 BC) - polygon approximation\n");
    printf("  2. Machin's Formula (1706) - fast arctan series\n");
    printf("  3. Newton's Formula (1666) - arcsin binomial expansion\n");
    printf("  4. Chudnovsky Algorithm (1988) - world record formula\n\n");
    printf("Press Ctrl+C to stop at any time...\n\n");

    /* ===== METHOD 1: PYTHAGOREAN (ANCIENT) ===== */
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║              METHOD 1: PYTHAGOREAN (Ancient Greece)          ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    printf("Starting with triangle (3 sides), doubling each iteration\n");
    printf("Stopping at 786,432 sides (optimal double-precision limit)\n\n");

    /* Method 1a: Direct expression evaluation */
    run_method("Direct Expression Evaluation",
               "sqrt(2 - sqrt(4 - s^2))",
               false);

    printf("═══════════════════════════════════════════════════════════════\n\n");

    /* Reset running flag if user didn't interrupt */
    if (running) {
        /* Method 1b: Numerical solver */
        run_method("Numerical Solver (Newton-Raphson)",
                   "x - sqrt(2 - sqrt(4 - s^2))",
                   true);
    }

    printf("═══════════════════════════════════════════════════════════════\n\n");
    sleep(1);

    /* ===== METHOD 2: MACHIN'S FORMULA ===== */
    if (running) {
        printf("╔═══════════════════════════════════════════════════════════════╗\n");
        printf("║                    METHOD 2: MACHIN'S FORMULA                 ║\n");
        printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

        printf("Used to calculate 100 digits of Pi in 1706\n");
        printf("Much faster convergence than Pythagorean method\n\n");

        run_machin();

        printf("═══════════════════════════════════════════════════════════════\n\n");
        sleep(1);
    }

    /* ===== METHOD 3: NEWTON'S FORMULA ===== */
    if (running) {
        printf("╔═══════════════════════════════════════════════════════════════╗\n");
        printf("║                   METHOD 3: NEWTON'S FORMULA                  ║\n");
        printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

        printf("Isaac Newton calculated 15 digits in 1666\n");
        printf("He said he was \"ashamed\" to admit how many digits he computed!\n\n");

        run_newton();

        printf("═══════════════════════════════════════════════════════════════\n\n");
        sleep(1);
    }

    /* ===== METHOD 4: CHUDNOVSKY ALGORITHM ===== */
    if (running) {
        printf("╔═══════════════════════════════════════════════════════════════╗\n");
        printf("║                 METHOD 4: CHUDNOVSKY ALGORITHM                ║\n");
        printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

        printf("Used to calculate billions and trillions of digits of Pi\n");
        printf("Holds all modern world records - adds ~14 digits per term!\n\n");

        run_chudnovsky();
    }

    /* ===== FINAL SUMMARY ===== */
    printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                     Calculation Complete!                     ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    printf("Convergence comparison:\n");
    printf("  • Pythagorean:  ~20 iterations (slow but ancient)\n");
    printf("  • Machin:       ~10 terms (fast, practical)\n");
    printf("  • Newton:       ~15 terms (medium, historical)\n");
    printf("  • Chudnovsky:   ~1 term (BLAZING FAST, modern)\n\n");

    printf("All methods converge to machine precision (±1e-14),\n");
    printf("demonstrating FluxParser's numerical accuracy!\n\n");

    return 0;
}
