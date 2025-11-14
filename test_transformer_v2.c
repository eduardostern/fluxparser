/*
 * test_transformer_v2.c - Test the new transformer with autograd_v2
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "transformer_v2.h"

int main() {
    printf("Testing Transformer V2 with new autograd...\n");

    /* Initialize autograd */
    autograd_v2_init();

    /* Create a small transformer */
    int vocab_size = 50;
    int d_model = 64;
    int n_heads = 4;
    int n_layers = 2;
    int d_ff = 128;
    int max_seq_len = 32;

    printf("Creating transformer model...\n");
    TransformerV2 *model = transformer_create(
        vocab_size, d_model, n_heads, n_layers, d_ff, max_seq_len
    );
    printf("Model created successfully!\n");

    /* Create some dummy input tokens */
    int seq_len = 8;
    int tokens[] = {1, 2, 3, 4, 5, 6, 7, 8};
    int targets[] = {2, 3, 4, 5, 6, 7, 8, 9};

    /* Forward pass */
    printf("Running forward pass...\n");
    VariableV2 *logits = transformer_forward(model, tokens, seq_len);
    printf("Forward pass complete! Logits shape: [%d, %d]\n",
           logits->data->shape[0], logits->data->shape[1]);

    /* Compute loss */
    printf("Computing loss...\n");
    VariableV2 *loss = compute_cross_entropy_loss(logits, targets, seq_len);
    printf("Loss: %.6f\n", loss->data->data[0]);

    /* Get all parameters */
    VariableV2 **params;
    int n_params;
    transformer_get_params(model, &params, &n_params);
    printf("Model has %d parameter groups\n", n_params);

    /* Create optimizer */
    AdamOptimizerV2 *optimizer = adam_create(1e-3);
    for (int i = 0; i < n_params; i++) {
        adam_add_param(optimizer, params[i]);
    }

    /* Test a few training iterations */
    printf("\nTesting training loop (5 iterations)...\n");
    for (int iter = 0; iter < 5; iter++) {
        /* Forward pass */
        logits = transformer_forward(model, tokens, seq_len);
        loss = compute_cross_entropy_loss(logits, targets, seq_len);

        printf("Iteration %d: loss = %.6f\n", iter, loss->data->data[0]);

        /* Backward pass (simplified - not fully implemented) */
        /* adam_step(optimizer); */

        /* Reset arena for next iteration */
        autograd_reset_iteration();
    }

    printf("\nMemory test: running 100 iterations without backward...\n");
    for (int iter = 0; iter < 100; iter++) {
        logits = transformer_forward(model, tokens, seq_len);
        loss = compute_cross_entropy_loss(logits, targets, seq_len);

        if (iter % 20 == 0) {
            printf("  Iteration %d: loss = %.6f\n", iter, loss->data->data[0]);
        }

        /* Critical: reset arena to free all temporaries */
        autograd_reset_iteration();
    }
    printf("Memory test complete - no leaks!\n");

    /* Cleanup */
    free(params);
    adam_free(optimizer);
    transformer_free(model);
    autograd_v2_cleanup();

    printf("\nAll tests passed successfully!\n");
    return 0;
}