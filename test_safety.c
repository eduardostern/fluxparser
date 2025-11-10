#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

void test_success() {
    printf("=== Test 1: Successful parsing ===\n");

    ParseResult result = parse_expression_safe("2 + 3 * 4");

    if (result.has_error) {
        printf("FAIL: Expected success but got error\n");
        parser_print_error("2 + 3 * 4", &result);
    } else {
        printf("PASS: Result = %.2f\n", result.value);
    }
    printf("\n");
}

void test_empty_expression() {
    printf("=== Test 2: Empty expression ===\n");

    ParseResult result = parse_expression_safe("");

    if (result.has_error) {
        printf("PASS: Error detected as expected\n");
        printf("Error code: %d (%s)\n", result.error.code,
               parser_error_string(result.error.code));
        printf("Message: %s\n", result.error.message);
    } else {
        printf("FAIL: Should have detected empty expression\n");
    }
    printf("\n");
}

void test_null_expression() {
    printf("=== Test 3: NULL expression ===\n");

    ParseResult result = parse_expression_safe(NULL);

    if (result.has_error) {
        printf("PASS: Error detected as expected\n");
        printf("Error code: %d (%s)\n", result.error.code,
               parser_error_string(result.error.code));
        printf("Message: %s\n", result.error.message);
    } else {
        printf("FAIL: Should have detected NULL expression\n");
    }
    printf("\n");
}

void test_too_long() {
    printf("=== Test 4: Expression too long ===\n");

    /* Create a very long expression */
    char *long_expr = malloc(PARSER_MAX_EXPR_LENGTH + 100);
    memset(long_expr, 'a', PARSER_MAX_EXPR_LENGTH + 50);
    long_expr[PARSER_MAX_EXPR_LENGTH + 50] = '\0';

    ParseResult result = parse_expression_safe(long_expr);

    if (result.has_error && result.error.code == PARSER_ERROR_TOO_LONG) {
        printf("PASS: Long expression rejected\n");
        printf("Message: %s\n", result.error.message);
    } else {
        printf("FAIL: Should have rejected long expression\n");
    }

    free(long_expr);
    printf("\n");
}

void test_too_deep() {
    printf("=== Test 5: Expression too deeply nested ===\n");

    /* Create deeply nested expression: (((((...))))) */
    char deep_expr[500] = {0};
    int depth = PARSER_MAX_DEPTH + 10;

    for (int i = 0; i < depth && i < 200; i++) {
        strcat(deep_expr, "(");
    }
    strcat(deep_expr, "1");
    for (int i = 0; i < depth && i < 200; i++) {
        strcat(deep_expr, ")");
    }

    ParseResult result = parse_expression_safe(deep_expr);

    if (result.has_error && result.error.code == PARSER_ERROR_TOO_DEEP) {
        printf("PASS: Deep nesting rejected\n");
        printf("Message: %s\n", result.error.message);
        printf("Position: %d\n", result.error.position);
    } else {
        printf("FAIL: Should have rejected deeply nested expression\n");
    }
    printf("\n");
}

void test_error_position() {
    printf("=== Test 6: Error position reporting ===\n");

    const char *expr = "2 + 3 * foo";
    ParseResult result = parse_expression_safe(expr);

    if (result.has_error) {
        printf("PASS: Error detected\n");
        parser_print_error(expr, &result);
    } else {
        printf("FAIL: Should have detected unknown identifier\n");
    }
    printf("\n");
}

void test_unknown_variable() {
    printf("=== Test 7: Unknown variable ===\n");

    double values[] = {5.0, 10.0};
    VarContext ctx = {
        .values = values,
        .count = 2,
        .mappings = NULL,
        .mapping_count = 0
    };

    const char *expr = "a + z";  /* z is out of range */
    ParseResult result = parse_expression_with_vars_safe(expr, &ctx);

    if (result.has_error) {
        printf("PASS: Unknown variable detected\n");
        parser_print_error(expr, &result);
    } else {
        printf("FAIL: Should have detected unknown variable\n");
    }
    printf("\n");
}

void test_syntax_error() {
    printf("=== Test 8: Syntax error position ===\n");

    const char *expr = "2 + (3 * 4";  /* Missing closing paren */
    ParseResult result = parse_expression_safe(expr);

    if (result.has_error) {
        printf("PASS: Syntax error detected\n");
        parser_print_error(expr, &result);
    } else {
        printf("FAIL: Should have detected syntax error\n");
    }
    printf("\n");
}

void test_multiple_expressions() {
    printf("=== Test 9: Batch processing with error checking ===\n");

    const char *expressions[] = {
        "2 + 3",
        "sqrt(16)",
        "2 / 0",
        "abs(-5)",
        "2 +",  /* Syntax error */
        "sin(pi / 2)"
    };

    int count = sizeof(expressions) / sizeof(expressions[0]);
    int passed = 0;

    for (int i = 0; i < count; i++) {
        ParseResult result = parse_expression_safe(expressions[i]);

        printf("Expression %d: %s\n", i + 1, expressions[i]);
        if (result.has_error) {
            printf("  ERROR: %s (at position %d)\n",
                   result.error.message, result.error.position);
        } else {
            printf("  Result: %.2f\n", result.value);
            passed++;
        }
    }

    printf("\nPassed: %d/%d\n", passed, count);
    printf("\n");
}

void test_comparison_old_new_api() {
    printf("=== Test 10: Comparing old vs new API ===\n");

    const char *expr = "2 + invalid";

    printf("Old API (returns 0.0 on error):\n");
    float old_result = parse_expression(expr);
    printf("  Returned: %.2f\n", old_result);

    printf("\nNew SAFE API (returns error info):\n");
    ParseResult new_result = parse_expression_safe(expr);
    if (new_result.has_error) {
        printf("  Error detected: %s\n", new_result.error.message);
        printf("  Can distinguish from valid result of 0.0!\n");
    } else {
        printf("  Result: %.2f\n", new_result.value);
    }
    printf("\n");
}

int main() {
    printf("==============================================\n");
    printf("  PARSER SAFETY FEATURE TESTS\n");
    printf("==============================================\n\n");

    test_success();
    test_empty_expression();
    test_null_expression();
    test_too_long();
    test_too_deep();
    test_error_position();
    test_unknown_variable();
    test_syntax_error();
    test_multiple_expressions();
    test_comparison_old_new_api();

    printf("==============================================\n");
    printf("All safety tests completed!\n");
    printf("==============================================\n");

    return 0;
}
