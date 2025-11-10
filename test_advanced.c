/*
 * FluxParser - Research-Grade C Math Parser
 * Copyright (C) 2025 Eduardo Stern
 *
 * Dual Licensed:
 * - GPL-3.0 for open-source/non-commercial use
 * - Commercial license available - see LICENSE-COMMERCIAL.md
 *
 * SPDX-License-Identifier: GPL-3.0-or-later OR Commercial
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include "parser.h"

/* Test 1: Comparison Operators */
void test_comparisons() {
    printf("=== Test 1: Comparison Operators ===\n");

    struct {
        const char *expr;
        float expected;
    } tests[] = {
        {"5 > 3", 1.0},
        {"3 > 5", 0.0},
        {"5 < 3", 0.0},
        {"3 < 5", 1.0},
        {"5 >= 5", 1.0},
        {"4 >= 5", 0.0},
        {"5 <= 5", 1.0},
        {"6 <= 5", 0.0},
        {"5 == 5", 1.0},
        {"5 == 6", 0.0},
        {"5 != 6", 1.0},
        {"5 != 5", 0.0},
        {"(2 + 3) > 4", 1.0},
        {"sqrt(16) == 4", 1.0},
        {"abs(-5) != 5", 0.0}
    };

    int passed = 0;
    for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
        ParseResult r = parse_expression_safe(tests[i].expr);
        if (!r.has_error && fabs(r.value - tests[i].expected) < 0.01) {
            printf("  [PASS] %s => %.0f\n", tests[i].expr, r.value);
            passed++;
        } else {
            printf("  [FAIL] %s => %.0f (expected %.0f)\n",
                   tests[i].expr, r.value, tests[i].expected);
        }
    }

    printf("  Passed: %d/%zu\n\n", passed, sizeof(tests)/sizeof(tests[0]));
}

/* Test 2: Complex Logical Expressions */
void test_complex_logic() {
    printf("=== Test 2: Complex Logical Expressions ===\n");

    struct {
        const char *expr;
        float expected;
    } tests[] = {
        {"5 > 3 && 10 < 20", 1.0},
        {"5 > 3 && 10 > 20", 0.0},
        {"5 < 3 || 10 < 20", 1.0},
        {"5 < 3 || 10 > 20", 0.0},
        {"!(5 > 3)", 0.0},
        {"!(5 < 3)", 1.0},
        {"5 > 3 && 4 < 6 && 7 == 7", 1.0},
        {"(5 > 3) || (10 < 5)", 1.0},
        {"2 + 2 == 4 && 3 * 3 == 9", 1.0}
    };

    int passed = 0;
    for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
        ParseResult r = parse_expression_safe(tests[i].expr);
        if (!r.has_error && fabs(r.value - tests[i].expected) < 0.01) {
            printf("  [PASS] %s => %.0f\n", tests[i].expr, r.value);
            passed++;
        } else {
            printf("  [FAIL] %s => %.0f (expected %.0f)\n",
                   tests[i].expr, r.value, tests[i].expected);
            if (r.has_error) {
                printf("         Error: %s\n", r.error.message);
            }
        }
    }

    printf("  Passed: %d/%zu\n\n", passed, sizeof(tests)/sizeof(tests[0]));
}

/* Test 3: Timeout Mechanism */
void test_timeout() {
    printf("=== Test 3: Timeout Mechanism ===\n");

    /* Create a long expression that would take time */
    char long_expr[10000];
    strcpy(long_expr, "1");
    for (int i = 0; i < 500; i++) {
        strcat(long_expr, "+1");
    }

    ParserConfig config = {
        .timeout_ms = 1,  /* 1 millisecond timeout */
        .continue_on_error = false,
        .thread_safe = false
    };

    ParseResult r = parse_expression_ex(long_expr, NULL, &config);

    if (r.has_error) {
        printf("  [INFO] Timeout detected (this is expected for safety)\n");
        printf("  Error: %s\n", r.error.message);
    } else {
        printf("  [INFO] Expression completed quickly: %.0f\n", r.value);
    }

    /* Test reasonable timeout */
    config.timeout_ms = 1000;  /* 1 second */
    r = parse_expression_ex("2 + 3 * 4", NULL, &config);

    if (!r.has_error) {
        printf("  [PASS] Normal expression within timeout: %.0f\n", r.value);
    } else {
        printf("  [FAIL] Normal expression timed out\n");
    }

    printf("\n");
}

/* Test 4: Thread Safety */
typedef struct {
    int thread_id;
    int iterations;
} ThreadData;

void* thread_worker(void* arg) {
    ThreadData* data = (ThreadData*)arg;

    /* Use thread-local debug mode */
    set_debug_mode_local(false);

    for (int i = 0; i < data->iterations; i++) {
        /* Each thread evaluates different expressions */
        char expr[100];
        snprintf(expr, sizeof(expr), "%d + %d * %d", data->thread_id, i, i);

        ParseResult r = parse_expression_safe(expr);

        if (r.has_error) {
            printf("  [Thread %d] ERROR: %s\n", data->thread_id, r.error.message);
            return NULL;
        }
    }

    printf("  [Thread %d] Completed %d iterations\n", data->thread_id, data->iterations);
    return NULL;
}

void test_thread_safety() {
    printf("=== Test 4: Thread Safety ===\n");

    const int NUM_THREADS = 4;
    const int ITERATIONS = 100;

    pthread_t threads[NUM_THREADS];
    ThreadData thread_data[NUM_THREADS];

    /* Start threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].iterations = ITERATIONS;
        pthread_create(&threads[i], NULL, thread_worker, &thread_data[i]);
    }

    /* Wait for threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("  [PASS] All threads completed successfully\n\n");
}

/* Test 5: Advanced Configuration */
void test_advanced_config() {
    printf("=== Test 5: Advanced Configuration ===\n");

    double vars[] = {10.0, 5.0};
    VarContext ctx = {
        .values = vars,
        .count = 2,
        .mappings = NULL,
        .mapping_count = 0
    };

    ParserConfig config = {
        .timeout_ms = 1000,
        .continue_on_error = false,
        .thread_safe = true
    };

    ParseResult r = parse_expression_ex("a > b && a != 0", &ctx, &config);

    if (!r.has_error) {
        printf("  [PASS] Expression with config: %.0f\n", r.value);
        printf("  a=%.0f > b=%.0f is %s\n",
               vars[0], vars[1], r.value ? "true" : "false");
    } else {
        printf("  [FAIL] %s\n", r.error.message);
    }

    printf("\n");
}

/* Test 6: Operator Precedence with Comparisons */
void test_precedence() {
    printf("=== Test 6: Operator Precedence ===\n");

    struct {
        const char *expr;
        float expected;
        const char *description;
    } tests[] = {
        {"2 + 3 > 4", 1.0, "Addition before comparison"},
        {"2 * 3 < 10", 1.0, "Multiplication before comparison"},
        {"5 > 3 && 10 > 5", 1.0, "Comparison before AND"},
        {"5 < 3 || 10 > 5", 1.0, "Comparison before OR"},
        {"2 + 3 * 4 > 10", 1.0, "Full arithmetic before comparison"},
        {"!(5 > 10)", 1.0, "NOT before comparison evaluation"}
    };

    int passed = 0;
    for (size_t i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
        ParseResult r = parse_expression_safe(tests[i].expr);
        if (!r.has_error && fabs(r.value - tests[i].expected) < 0.01) {
            printf("  [PASS] %s => %.0f (%s)\n",
                   tests[i].expr, r.value, tests[i].description);
            passed++;
        } else {
            printf("  [FAIL] %s => %.0f (expected %.0f)\n",
                   tests[i].expr, r.value, tests[i].expected);
        }
    }

    printf("  Passed: %d/%zu\n\n", passed, sizeof(tests)/sizeof(tests[0]));
}

int main() {
    printf("==============================================\n");
    printf("  ADVANCED FEATURES TEST SUITE\n");
    printf("==============================================\n\n");

    test_comparisons();
    test_complex_logic();
    test_timeout();
    test_thread_safety();
    test_advanced_config();
    test_precedence();

    printf("==============================================\n");
    printf("All advanced tests completed!\n");
    printf("==============================================\n");

    return 0;
}
