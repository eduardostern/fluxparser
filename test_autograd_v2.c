/*
 * Test program for Autograd V2
 * Verifies memory management and basic training works
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "autograd_v2.h"
#include "arena.h"

int main(void) {
    printf("=== AUTOGRAD V2 TEST ===\n\n");

    srand(time(NULL));
    autograd_v2_init();

    printf("Creating simple neural network...\n");

    /* Create a simple 2-layer network */
    Linear *layer1 = linear_create(10, 32);
    Linear *layer2 = linear_create(32, 5);

    /* Collect parameters */
    VariableV2 *params[] = {
        layer1->weight, layer1->bias,
        layer2->weight, layer2->bias
    };
    int num_params = 4;

    OptimizerV2 *optimizer = optimizer_create(params, num_params, 0.01);

    printf("Initial arena allocated: %zu bytes\n\n", arena_get_allocated(global_arena));

    /* Training loop */
    printf("Running 100 iterations (should not leak memory)...\n");

    for (int iter = 0; iter < 100; iter++) {
        optimizer_zero_grad(optimizer);

        /* Create random input */
        int input_shape[] = {4, 10};  /* batch=4, features=10 */
        TensorV2 *x_tensor = tensor_create_temp(input_shape, 2);
        for (int i = 0; i < 40; i++) {
            x_tensor->data[i] = ((double)rand() / RAND_MAX) - 0.5;
        }
        VariableV2 *x = var_create_temp(x_tensor, false);

        /* Forward pass */
        VariableV2 *h1 = linear_forward(layer1, x);
        VariableV2 *h1_relu = ag_relu(h1);
        VariableV2 *output = linear_forward(layer2, h1_relu);

        /* Create random target */
        int target_shape[] = {4, 5};
        TensorV2 *target_tensor = tensor_zeros_temp(target_shape, 2);
        for (int b = 0; b < 4; b++) {
            int class_idx = rand() % 5;
            target_tensor->data[b * 5 + class_idx] = 1.0;
        }
        VariableV2 *target = var_create_temp(target_tensor, false);

        /* Compute loss (simplified - just use MSE) */
        TensorV2 *diff = tensor_subtract(output->data, target->data);
        double loss = 0.0;
        for (int i = 0; i < diff->size; i++) {
            loss += diff->data[i] * diff->data[i];
        }
        loss /= diff->size;

        /* Backward pass */
        tape_backward(g_tape);
        optimizer_step(optimizer);

        if ((iter + 1) % 10 == 0) {
            printf("  Iter %3d: Loss = %.4f | Arena used: %zu bytes\n",
                   iter + 1, loss, arena_get_used(global_arena));
        }

        /* CRITICAL: Reset iteration to free all temporaries */
        autograd_reset_iteration();
    }

    printf("\n✅ Test complete!\n");
    printf("Final arena allocated: %zu bytes\n", arena_get_allocated(global_arena));
    printf("Arena used after reset: %zu bytes\n\n", arena_get_used(global_arena));

    if (arena_get_used(global_arena) == 0) {
        printf("SUCCESS: No memory leak - arena properly reset!\n");
    } else {
        printf("WARNING: Arena not fully reset\n");
    }

    /* Cleanup */
    optimizer_destroy(optimizer);
    autograd_v2_cleanup();

    return 0;
}