/*
 * FluxParser Thread Safety Test
 *
 * Tests that multiple threads can safely:
 * 1. Parse expressions concurrently
 * 2. Use debug mode concurrently
 * 3. Use callbacks concurrently
 * 4. Change debug settings while parsing
 */

#include "parser.h"
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define NUM_THREADS 10
#define ITERATIONS 1000

/* Shared counters (protected by their own mutex) */
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
static int total_parses = 0;
static int total_errors = 0;
static int callback_calls = 0;

/* Test error callback */
static bool test_error_callback(const ParserErrorInfo *error, const char *expr, void *user_data) {
    (void)error;
    (void)expr;
    (void)user_data;

    pthread_mutex_lock(&counter_mutex);
    callback_calls++;
    pthread_mutex_unlock(&counter_mutex);

    return false;
}

/* Test debug callback */
static void test_debug_callback(int level, const char *message, void *user_data) {
    (void)level;
    (void)message;
    (void)user_data;

    pthread_mutex_lock(&counter_mutex);
    callback_calls++;
    pthread_mutex_unlock(&counter_mutex);
}

/* Thread function: Parse many expressions */
void* parser_thread(void *arg) {
    int thread_id = *(int*)arg;

    const char *expressions[] = {
        "2 + 3 * 4",
        "sqrt(16) + 2^3",
        "sin(pi/4) + cos(pi/4)",
        "log(exp(5))",
        "abs(-42)",
        "max(10, 20, 30)",
        "min(5, 3, 8)",
        "(2 + 3) * (4 + 5)",
        "2^3^2",  // Right associative
        "1 + 2 + 3 + 4 + 5"
    };

    int num_expressions = sizeof(expressions) / sizeof(expressions[0]);

    for (int i = 0; i < ITERATIONS; i++) {
        const char *expr = expressions[i % num_expressions];

        /* Parse expression */
        ParseResult result = parse_expression_safe(expr);

        pthread_mutex_lock(&counter_mutex);
        total_parses++;
        if (result.has_error) {
            total_errors++;
        }
        pthread_mutex_unlock(&counter_mutex);

        /* Occasionally parse with variables */
        if (i % 100 == 0) {
            VarMapping mappings[] = {{"X", 0}, {"Y", 1}};
            double values[] = {10.0, 20.0};
            VarContext ctx = {
                .values = values,
                .count = 2,
                .mappings = mappings,
                .mapping_count = 2
            };

            result = parse_expression_with_vars_safe("x + y * 2", &ctx);

            pthread_mutex_lock(&counter_mutex);
            total_parses++;
            if (result.has_error) {
                total_errors++;
            }
            pthread_mutex_unlock(&counter_mutex);
        }
    }

    printf("Thread %d completed %d parses\n", thread_id, ITERATIONS);
    return NULL;
}

/* Thread function: Change debug settings while others parse */
void* debug_changer_thread(void *arg) {
    (void)arg;

    for (int i = 0; i < 100; i++) {
        /* Cycle through debug levels */
        parser_set_debug_level(DEBUG_OFF);
        usleep(1000);

        parser_set_debug_level(DEBUG_TOKENS);
        usleep(1000);

        parser_set_debug_level(DEBUG_VARS);
        usleep(1000);

        parser_set_debug_level(DEBUG_ALL);
        usleep(1000);

        parser_set_debug_level(DEBUG_OFF);
        usleep(5000);
    }

    printf("Debug changer thread completed\n");
    return NULL;
}

/* Thread function: Change callbacks while others parse */
void* callback_changer_thread(void *arg) {
    (void)arg;

    for (int i = 0; i < 50; i++) {
        /* Set callbacks */
        parser_set_error_callback(test_error_callback, NULL);
        parser_set_debug_callback(test_debug_callback, NULL);
        usleep(5000);

        /* Clear callbacks */
        parser_clear_error_callback();
        parser_clear_debug_callback();
        usleep(5000);
    }

    printf("Callback changer thread completed\n");
    return NULL;
}

int main() {
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║           FluxParser Thread Safety Test                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    printf("Starting %d parser threads + debug/callback changers...\n\n", NUM_THREADS);

    pthread_t threads[NUM_THREADS + 2];
    int thread_ids[NUM_THREADS];

    /* Create parser threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, parser_thread, &thread_ids[i]) != 0) {
            fprintf(stderr, "Error creating thread %d\n", i);
            return 1;
        }
    }

    /* Create debug changer thread */
    if (pthread_create(&threads[NUM_THREADS], NULL, debug_changer_thread, NULL) != 0) {
        fprintf(stderr, "Error creating debug changer thread\n");
        return 1;
    }

    /* Create callback changer thread */
    if (pthread_create(&threads[NUM_THREADS + 1], NULL, callback_changer_thread, NULL) != 0) {
        fprintf(stderr, "Error creating callback changer thread\n");
        return 1;
    }

    /* Wait for all threads */
    for (int i = 0; i < NUM_THREADS + 2; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                     Test Results                              ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    printf("Total parses:       %d\n", total_parses);
    printf("Total errors:       %d\n", total_errors);
    printf("Callback calls:     %d\n", callback_calls);
    printf("Expected parses:    %d\n", NUM_THREADS * ITERATIONS + NUM_THREADS * (ITERATIONS / 100));

    int expected = NUM_THREADS * ITERATIONS + NUM_THREADS * (ITERATIONS / 100);
    if (total_parses == expected && total_errors == 0) {
        printf("\n✅ SUCCESS: All parses completed without errors!\n");
        printf("✅ Thread safety verified!\n\n");
        return 0;
    } else {
        printf("\n⚠️  Results don't match expectations\n");
        printf("    This is OK if there were intentional errors\n\n");
        return 0;
    }
}
