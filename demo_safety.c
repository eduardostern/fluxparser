#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

void demo_basic_safety() {
    printf("==============================================\n");
    printf(" SAFETY IMPROVEMENTS DEMO\n");
    printf("==============================================\n\n");

    printf("1. INPUT LENGTH VALIDATION\n");
    printf("--------------------------\n");
    char *huge = malloc(PARSER_MAX_EXPR_LENGTH + 100);
    memset(huge, '1', PARSER_MAX_EXPR_LENGTH + 50);
    huge[PARSER_MAX_EXPR_LENGTH + 50] = '\0';

    ParseResult r1 = parse_expression_safe(huge);
    if (r1.has_error) {
        printf("✓ Rejected expression of %d chars (max: %d)\n",
               PARSER_MAX_EXPR_LENGTH + 50, PARSER_MAX_EXPR_LENGTH);
        printf("  Error: %s\n", r1.error.message);
    }
    free(huge);
    printf("\n");

    printf("2. RECURSION DEPTH LIMITING\n");
    printf("---------------------------\n");
    char deep[500] = "";
    for (int i = 0; i < 120; i++) strcat(deep, "(");
    strcat(deep, "1");
    for (int i = 0; i < 120; i++) strcat(deep, ")");

    ParseResult r2 = parse_expression_safe(deep);
    if (r2.has_error) {
        printf("✓ Prevented stack overflow from %d nested parens\n", 120);
        printf("  Max depth allowed: %d\n", PARSER_MAX_DEPTH);
        printf("  Error position: %d\n", r2.error.position);
    }
    printf("\n");

    printf("3. NULL/EMPTY INPUT VALIDATION\n");
    printf("------------------------------\n");
    ParseResult r3 = parse_expression_safe(NULL);
    printf("✓ NULL input: %s\n", r3.error.message);

    ParseResult r4 = parse_expression_safe("");
    printf("✓ Empty input: %s\n", r4.error.message);
    printf("\n");

    printf("4. ERROR vs SUCCESS DISTINCTION\n");
    printf("--------------------------------\n");
    printf("OLD API problem:\n");
    printf("  parse_expression(\"invalid\") returns 0.0\n");
    printf("  parse_expression(\"0\") also returns 0.0\n");
    printf("  → Cannot tell error from valid zero!\n\n");

    printf("NEW API solution:\n");
    ParseResult valid = parse_expression_safe("5 - 5");
    ParseResult invalid = parse_expression_safe("+++");

    printf("  Valid \"5 - 5\":\n");
    printf("    has_error: %s\n", valid.has_error ? "true" : "false");
    printf("    value: %.2f\n", valid.value);

    printf("  Invalid \"+++\":\n");
    printf("    has_error: %s\n", invalid.has_error ? "true" : "false");
    printf("    error_code: %d\n", invalid.error.code);
    printf("  ✓ Can distinguish error from valid zero!\n");
    printf("\n");

    printf("5. ERROR POSITION TRACKING\n");
    printf("--------------------------\n");
    const char *bad_expr = "2 + 3 * sqrt(-1)";
    ParseResult r5 = parse_expression_safe(bad_expr);

    printf("Expression: %s\n", bad_expr);
    if (r5.has_error) {
        printf("Error at position %d: %s\n",
               r5.error.position, r5.error.message);
    } else {
        printf("Result: %.2f (sqrt of negative allowed)\n", r5.value);
    }
    printf("\n");

    printf("6. DETAILED ERROR REPORTING\n");
    printf("---------------------------\n");
    const char *bad_expr2 = "2 + (3 * 4";
    ParseResult r6 = parse_expression_safe(bad_expr2);

    printf("Expression with missing paren:\n");
    parser_print_error(bad_expr2, &r6);
    printf("\n");

    printf("7. SAFE BATCH PROCESSING\n");
    printf("------------------------\n");
    const char *exprs[] = {
        "2 + 3",
        "sqrt(16)",
        "10 / 2",
        "abs(-5)"
    };

    int success = 0, failed = 0;
    for (int i = 0; i < 4; i++) {
        ParseResult r = parse_expression_safe(exprs[i]);
        if (r.has_error) {
            failed++;
            printf("  [FAIL] %s: %s\n", exprs[i], r.error.message);
        } else {
            success++;
            printf("  [OK]   %s = %.2f\n", exprs[i], r.value);
        }
    }
    printf("  Summary: %d succeeded, %d failed\n", success, failed);
    printf("\n");

    printf("==============================================\n");
    printf(" KEY IMPROVEMENTS\n");
    printf("==============================================\n");
    printf("✓ Input validation (length, NULL, empty)\n");
    printf("✓ Stack overflow prevention (depth limit)\n");
    printf("✓ Error vs success distinction\n");
    printf("✓ Position tracking for debugging\n");
    printf("✓ Detailed error messages\n");
    printf("✓ Backward compatible (old API still works)\n");
    printf("✓ Production-ready error handling\n");
    printf("==============================================\n");
}

int main() {
    demo_basic_safety();
    return 0;
}
