/*
 * test_minimal_crash.c - Find exact crash location
 */
#include <stdio.h>
#include <stdlib.h>
#include "transformer_v2.h"

int main() {
    printf("Finding crash location...\n\n");

    /* Initialize autograd */
    printf("1. Initializing autograd...\n");
    autograd_v2_init();

    /* Create minimal transformer */
    printf("2. Creating transformer...\n");
    TransformerV2 *model = transformer_create(10, 8, 2, 1, 16, 8);
    printf("   Model created\n");

    /* Create input */
    printf("3. Creating input...\n");
    int tokens[] = {1, 2};
    int targets[] = {2, 3};

    /* Forward pass */
    printf("4. Running forward pass...\n");
    VariableV2 *logits = transformer_forward(model, tokens, 2);
    printf("   Forward complete\n");

    /* Compute loss */
    printf("5. Computing loss...\n");
    VariableV2 *loss = compute_cross_entropy_loss(logits, targets, 2);
    printf("   Loss = %.4f\n", loss->data->data[0]);

    /* Set gradient */
    printf("6. Setting loss gradient...\n");
    loss->grad->data[0] = 1.0;
    printf("   Gradient set\n");

    /* Backward pass */
    printf("7. Running backward pass...\n");
    tape_backward(g_tape);
    printf("   Backward complete\n");

    /* Reset */
    printf("8. Resetting tape...\n");
    tape_reset(g_tape);
    printf("   Tape reset\n");

    printf("9. Resetting arena...\n");
    autograd_reset_iteration();
    printf("   Arena reset\n");

    printf("\n✅ All operations completed without crash!\n");
    printf("The crash happens during cleanup.\n");

    return 0;
}