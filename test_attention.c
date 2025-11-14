/*
 * test_attention.c - Test multi-head attention forward/backward
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "transformer_v2.h"

int main() {
    printf("Testing Multi-Head Attention...\n\n");

    /* Initialize autograd */
    autograd_v2_init();

    /* Create attention layer */
    int d_model = 8;
    int n_heads = 2;

    printf("Creating attention: d_model=%d, n_heads=%d\n", d_model, n_heads);
    MultiHeadAttention *mha = mha_create(d_model, n_heads);

    /* Create input */
    int seq_len = 3;
    int shape[] = {seq_len, d_model};
    TensorV2 *x_tensor = tensor_create_temp(shape, 2);

    /* Initialize with small values */
    for (int i = 0; i < seq_len * d_model; i++) {
        x_tensor->data[i] = ((double)rand() / RAND_MAX) * 0.1;
    }

    VariableV2 *x = var_create_temp(x_tensor, true);

    printf("Input shape: [%d, %d]\n", seq_len, d_model);

    /* Forward pass */
    printf("\nRunning forward pass...\n");
    VariableV2 *y = mha_forward(mha, x);
    printf("Output shape: [%d, %d]\n", y->data->shape[0], y->data->shape[1]);

    /* Check if output has reasonable values */
    double sum = 0.0;
    for (int i = 0; i < y->data->size; i++) {
        sum += fabs(y->data->data[i]);
    }
    printf("Output sum of abs values: %.4f\n", sum);

    /* Set gradient on output */
    printf("\nSetting output gradient...\n");
    for (int i = 0; i < y->data->size; i++) {
        y->grad->data[i] = 1.0 / y->data->size;
    }

    /* Backward pass */
    printf("Running backward pass...\n");
    tape_backward(g_tape);

    /* Check if any gradients were computed */
    double grad_sum = 0.0;
    if (mha->q_proj->weight->grad) {
        for (int i = 0; i < mha->q_proj->weight->grad->size; i++) {
            grad_sum += fabs(mha->q_proj->weight->grad->data[i]);
        }
    }
    printf("Q projection weight gradient sum: %.4f\n", grad_sum);

    printf("\n✅ Attention test complete!\n");

    /* Cleanup */
    mha_free(mha);
    autograd_reset_iteration();
    autograd_v2_cleanup();

    return 0;
}