/*
 * FluxParser Debug Mode Demo
 *
 * Demonstrates:
 * 1. All debug levels (TOKENS, VARS, EVAL, etc.)
 * 2. Error callbacks for custom error handling
 * 3. Debug callbacks for custom debug output
 * 4. Combining multiple debug flags
 */

#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Custom error handler */
static int error_count = 0;
static bool custom_error_handler(const ParserErrorInfo *error, const char *expr, void *user_data) {
    int *count = (int*)user_data;
    (*count)++;

    printf("\n");
    printf("┌─────────────────────────────────────────────────────────────┐\n");
    printf("│ ⚠️  CUSTOM ERROR HANDLER                                     │\n");
    printf("├─────────────────────────────────────────────────────────────┤\n");
    printf("│ Error #%d: %s\n", *count, parser_error_string(error->code));
    printf("│ Position: %d\n", error->position);
    printf("│ Message: %s\n", error->message);
    printf("│ Expression: %s\n", expr);
    printf("│             %*s^ HERE\n", error->position, "");
    printf("└─────────────────────────────────────────────────────────────┘\n");

    return false;  /* Don't continue after error */
}

/* Custom debug handler */
static void custom_debug_handler(int level, const char *message, void *user_data) {
    FILE *log = (FILE*)user_data;

    const char *level_name = "UNKNOWN";
    if (level & DEBUG_TOKENS) level_name = "TOKENS";
    else if (level & DEBUG_AST) level_name = "AST";
    else if (level & DEBUG_EVAL) level_name = "EVAL";
    else if (level & DEBUG_VARS) level_name = "VARS";
    else if (level & DEBUG_FUNCS) level_name = "FUNCS";
    else if (level & DEBUG_OPTIMIZE) level_name = "OPTIMIZE";
    else if (level & DEBUG_TIMING) level_name = "TIMING";

    fprintf(log, "[%-8s] %s", level_name, message);
    fflush(log);
}

void print_section(const char *title) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║ %-61s ║\n", title);
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
}

int main() {
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║          FluxParser: Debug Mode & Callbacks Demo             ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    /* ========== TEST 1: DEBUG_TOKENS ========== */
    print_section("TEST 1: DEBUG_TOKENS - Show tokenization");

    parser_set_debug_level(DEBUG_TOKENS);
    ParseResult result = parse_expression_safe("2 + 3 * 4");
    printf("Result: %.2f\n", result.value);
    parser_set_debug_level(DEBUG_OFF);

    /* ========== TEST 2: DEBUG_VARS ========== */
    print_section("TEST 2: DEBUG_VARS - Show variable lookups");

    parser_set_debug_level(DEBUG_VARS);

    VarMapping mappings[] = {{"X", 0}, {"Y", 1}};
    double values[] = {10.0, 20.0};
    VarContext ctx = {
        .values = values,
        .count = 2,
        .mappings = mappings,
        .mapping_count = 2
    };

    result = parse_expression_with_vars_safe("x + y * 2", &ctx);
    printf("Result: %.2f\n", result.value);
    parser_set_debug_level(DEBUG_OFF);

    /* ========== TEST 3: MULTIPLE DEBUG FLAGS ========== */
    print_section("TEST 3: Combine DEBUG_TOKENS | DEBUG_VARS");

    parser_set_debug_level(DEBUG_TOKENS | DEBUG_VARS);
    result = parse_expression_with_vars_safe("sqrt(x^2 + y^2)", &ctx);
    printf("Result: %.2f\n", result.value);
    parser_set_debug_level(DEBUG_OFF);

    /* ========== TEST 4: DEBUG ALL ========== */
    print_section("TEST 4: DEBUG_ALL - Enable everything");

    parser_set_debug_level(DEBUG_ALL);
    result = parse_expression_safe("sin(pi/4) + cos(pi/4)");
    printf("Result: %.6f\n", result.value);
    parser_set_debug_level(DEBUG_OFF);

    /* ========== TEST 5: CUSTOM ERROR CALLBACK ========== */
    print_section("TEST 5: Custom Error Callback");

    error_count = 0;
    parser_set_error_callback(custom_error_handler, &error_count);

    printf("Parsing invalid expression: \"2 + + 3\"\n");
    result = parse_expression_safe("2 + + 3");

    printf("\nParsing invalid expression: \"sqrt(-1\"\n");
    result = parse_expression_safe("sqrt(-1");

    printf("\nTotal errors caught: %d\n", error_count);
    parser_clear_error_callback();

    /* ========== TEST 6: CUSTOM DEBUG CALLBACK ========== */
    print_section("TEST 6: Custom Debug Callback (logging to file)");

    FILE *debug_log = fopen("debug.log", "w");
    if (debug_log) {
        fprintf(debug_log, "=== FluxParser Debug Log ===\n\n");

        parser_set_debug_level(DEBUG_TOKENS | DEBUG_VARS);
        parser_set_debug_callback(custom_debug_handler, debug_log);

        printf("Parsing: \"x^2 + 2*x + 1\" with x=5\n");
        printf("(Debug output going to debug.log)\n\n");

        values[0] = 5.0;
        result = parse_expression_with_vars_safe("x^2 + 2*x + 1", &ctx);

        fprintf(debug_log, "\n=== Result: %.2f ===\n", result.value);
        fclose(debug_log);

        printf("Result: %.2f\n", result.value);
        printf("Debug log written to: debug.log\n");

        /* Show log contents */
        printf("\nLog contents:\n");
        printf("─────────────────────────────────────────\n");
        system("cat debug.log");
        printf("─────────────────────────────────────────\n");

        parser_clear_debug_callback();
        parser_set_debug_level(DEBUG_OFF);
    }

    /* ========== TEST 7: ERROR + DEBUG CALLBACKS TOGETHER ========== */
    print_section("TEST 7: Error & Debug Callbacks Together");

    error_count = 0;
    parser_set_error_callback(custom_error_handler, &error_count);
    parser_set_debug_level(DEBUG_TOKENS);

    printf("Parsing: \"log(-5) + sqrt(x)\" with x=16\n\n");
    values[0] = 16.0;
    result = parse_expression_with_vars_safe("log(-5) + sqrt(x)", &ctx);

    if (!result.has_error) {
        printf("\nResult: %.2f\n", result.value);
    }

    parser_clear_error_callback();
    parser_set_debug_level(DEBUG_OFF);

    /* ========== TEST 8: PRODUCTION MODE ========== */
    print_section("TEST 8: Production Mode (no debug, custom error handler)");

    printf("In production, you might want:\n");
    printf("  - Error callback for logging/monitoring\n");
    printf("  - No debug output (performance)\n");
    printf("  - Graceful error recovery\n\n");

    error_count = 0;
    parser_set_error_callback(custom_error_handler, &error_count);
    parser_set_debug_level(DEBUG_OFF);

    const char *expressions[] = {
        "2 + 3 * 4",
        "sqrt(16) + 2^3",
        "sin(pi/2)",
        "1 / 0",  // Error!
        "unknown_func(5)",  // Error!
        "2 + 2"
    };

    printf("Processing %zu expressions:\n", sizeof(expressions)/sizeof(expressions[0]));
    for (size_t i = 0; i < sizeof(expressions)/sizeof(expressions[0]); i++) {
        result = parse_expression_safe(expressions[i]);
        if (!result.has_error) {
            printf("  ✓ \"%s\" = %.2f\n", expressions[i], result.value);
        }
    }

    printf("\nTotal errors in batch: %d\n", error_count);
    parser_clear_error_callback();

    /* ========== SUMMARY ========== */
    print_section("Summary: Debug & Callback Features");

    printf("Debug Levels (can be OR'ed):\n");
    printf("  DEBUG_TOKENS    (1)  - Show tokenization\n");
    printf("  DEBUG_AST       (2)  - Show AST structure\n");
    printf("  DEBUG_EVAL      (4)  - Show evaluation steps\n");
    printf("  DEBUG_VARS      (8)  - Show variable lookups\n");
    printf("  DEBUG_FUNCS    (16)  - Show function calls\n");
    printf("  DEBUG_OPTIMIZE (32)  - Show optimization\n");
    printf("  DEBUG_TIMING   (64)  - Show timing info\n");
    printf("  DEBUG_ALL     (255)  - Enable all\n\n");

    printf("Callback Features:\n");
    printf("  ✓ Custom error handling (logging, recovery, UI)\n");
    printf("  ✓ Custom debug output (files, structured logging)\n");
    printf("  ✓ User data context for state tracking\n");
    printf("  ✓ Production-ready error monitoring\n\n");

    printf("Use Cases:\n");
    printf("  • Development: Enable DEBUG_ALL for deep inspection\n");
    printf("  • Testing: Use error callbacks to collect all errors\n");
    printf("  • Production: Error callbacks for monitoring/alerting\n");
    printf("  • GUI Apps: Callbacks for user-friendly error display\n\n");

    printf("✅ Demo complete!\n\n");

    return 0;
}
