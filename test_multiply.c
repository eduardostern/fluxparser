/*
 * test_multiply.c - Test just multiply operation
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "autograd_v2.h"

int main() {
    printf("Testing multiply operation...\n\n");

    /* Initialize autograd */
    autograd_v2_init();

    /* Create two parameters */
    int shape[] = {2};
    TensorV2 *a_data = tensor_create_persistent(shape, 1);
    TensorV2 *b_data = tensor_create_persistent(shape, 1);

    a_data->data[0] = 3.0;
    a_data->data[1] = 4.0;

    b_data->data[0] = 5.0;
    b_data->data[1] = 6.0;

    VariableV2 *a = var_create_parameter(a_data);
    VariableV2 *b = var_create_parameter(b_data);

    printf("a = [%.1f, %.1f]\n", a->data->data[0], a->data->data[1]);
    printf("b = [%.1f, %.1f]\n", b->data->data[0], b->data->data[1]);

    /* Forward: c = a * b */
    VariableV2 *c = ag_multiply(a, b);
    printf("c = a * b = [%.1f, %.1f]\n", c->data->data[0], c->data->data[1]);

    /* Set output gradient */
    c->grad->data[0] = 1.0;
    c->grad->data[1] = 1.0;

    /* Backward */
    tape_backward(g_tape);

    /* Check gradients */
    printf("\nGradients after backward:\n");
    printf("a.grad = [%.1f, %.1f] (should be [5.0, 6.0])\n", a->grad->data[0], a->grad->data[1]);
    printf("b.grad = [%.1f, %.1f] (should be [3.0, 4.0])\n", b->grad->data[0], b->grad->data[1]);

    printf("\n✅ Test complete!\n");

    /* Cleanup */
    var_free_persistent(a);
    var_free_persistent(b);
    autograd_v2_cleanup();

    return 0;
}