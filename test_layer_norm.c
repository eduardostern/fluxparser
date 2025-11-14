/*
 * test_layer_norm.c - Test layer normalization forward and backward
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "transformer_v2.h"

int main() {
    printf("Testing Layer Normalization...\n\n");

    /* Initialize autograd */
    autograd_v2_init();

    /* Create layer norm */
    int size = 4;
    LayerNormV2 *ln = layer_norm_create(size);

    /* Create input */
    int shape[] = {2, 4};  /* 2 positions, 4 features */
    TensorV2 *x_tensor = tensor_create_temp(shape, 2);

    /* Initialize with values */
    x_tensor->data[0] = 1.0; x_tensor->data[1] = 2.0;
    x_tensor->data[2] = 3.0; x_tensor->data[3] = 4.0;
    x_tensor->data[4] = 5.0; x_tensor->data[5] = 6.0;
    x_tensor->data[6] = 7.0; x_tensor->data[7] = 8.0;

    VariableV2 *x = var_create_temp(x_tensor, true);

    printf("Input:\n");
    for (int i = 0; i < 2; i++) {
        printf("  Position %d: [", i);
        for (int j = 0; j < 4; j++) {
            printf("%.1f%s", x_tensor->data[i*4 + j], j < 3 ? ", " : "");
        }
        printf("]\n");
    }

    /* Forward pass */
    printf("\nRunning forward pass...\n");
    VariableV2 *y = layer_norm_forward(ln, x);

    printf("Output after layer norm:\n");
    for (int i = 0; i < 2; i++) {
        printf("  Position %d: [", i);
        for (int j = 0; j < 4; j++) {
            printf("%.3f%s", y->data->data[i*4 + j], j < 3 ? ", " : "");
        }
        printf("]\n");
    }

    /* Set gradient on output */
    printf("\nSetting output gradient to all 1s...\n");
    for (int i = 0; i < 8; i++) {
        y->grad->data[i] = 1.0;
    }

    /* Backward pass */
    printf("Running backward pass...\n");
    tape_backward(g_tape);

    /* Check gradients */
    printf("\nGradients:\n");
    printf("  Gamma gradient: [");
    for (int i = 0; i < 4; i++) {
        printf("%.3f%s", ln->gamma->grad->data[i], i < 3 ? ", " : "");
    }
    printf("]\n");

    printf("  Beta gradient: [");
    for (int i = 0; i < 4; i++) {
        printf("%.3f%s", ln->beta->grad->data[i], i < 3 ? ", " : "");
    }
    printf("]\n");

    printf("\n✅ Layer norm test complete!\n");

    /* Cleanup */
    layer_norm_free(ln);
    autograd_reset_iteration();
    autograd_v2_cleanup();

    return 0;
}