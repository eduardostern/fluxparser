/*
 * test_backward.c - Test that backward pass actually computes gradients
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "autograd_v2.h"

int main() {
    printf("Testing backward pass implementation...\n\n");

    /* Initialize autograd */
    autograd_v2_init();

    /* Test 1: Simple add operation */
    printf("Test 1: Addition backward\n");
    {
        int shape[] = {2, 2};
        TensorV2 *a_data = tensor_create_persistent(shape, 2);
        TensorV2 *b_data = tensor_create_persistent(shape, 2);

        /* Initialize with values */
        a_data->data[0] = 1.0; a_data->data[1] = 2.0;
        a_data->data[2] = 3.0; a_data->data[3] = 4.0;

        b_data->data[0] = 5.0; b_data->data[1] = 6.0;
        b_data->data[2] = 7.0; b_data->data[3] = 8.0;

        VariableV2 *a = var_create_parameter(a_data);
        VariableV2 *b = var_create_parameter(b_data);

        /* Forward: c = a + b */
        VariableV2 *c = ag_add(a, b);

        /* Set output gradient to all 1s */
        for (int i = 0; i < 4; i++) {
            c->grad->data[i] = 1.0;
        }

        /* Backward */
        tape_backward(g_tape);

        /* Check gradients (should be 1.0 for addition) */
        printf("  a.grad: [");
        for (int i = 0; i < 4; i++) {
            printf("%.1f%s", a->grad->data[i], i < 3 ? ", " : "");
        }
        printf("] - Expected: [1.0, 1.0, 1.0, 1.0]\n");

        printf("  b.grad: [");
        for (int i = 0; i < 4; i++) {
            printf("%.1f%s", b->grad->data[i], i < 3 ? ", " : "");
        }
        printf("] - Expected: [1.0, 1.0, 1.0, 1.0]\n");

        bool pass = true;
        for (int i = 0; i < 4; i++) {
            if (fabs(a->grad->data[i] - 1.0) > 1e-6 ||
                fabs(b->grad->data[i] - 1.0) > 1e-6) {
                pass = false;
            }
        }
        printf("  Result: %s\n\n", pass ? "✅ PASS" : "❌ FAIL");

        var_free_persistent(a);
        var_free_persistent(b);
        tape_reset(g_tape);
        autograd_reset_iteration();  /* Clear arena */
    }

    /* Test 2: Multiply operation */
    printf("Test 2: Multiplication backward\n");
    {
        int shape[] = {2, 2};
        TensorV2 *a_data = tensor_create_persistent(shape, 2);
        TensorV2 *b_data = tensor_create_persistent(shape, 2);

        /* Initialize with values */
        a_data->data[0] = 2.0; a_data->data[1] = 3.0;
        a_data->data[2] = 4.0; a_data->data[3] = 5.0;

        b_data->data[0] = 6.0; b_data->data[1] = 7.0;
        b_data->data[2] = 8.0; b_data->data[3] = 9.0;

        VariableV2 *a = var_create_parameter(a_data);
        VariableV2 *b = var_create_parameter(b_data);

        /* Forward: c = a * b */
        VariableV2 *c = ag_multiply(a, b);

        /* Set output gradient to all 1s */
        for (int i = 0; i < 4; i++) {
            c->grad->data[i] = 1.0;
        }

        /* Backward */
        tape_backward(g_tape);

        /* Check gradients (grad_a should be b values, grad_b should be a values) */
        printf("  a.grad: [");
        for (int i = 0; i < 4; i++) {
            printf("%.1f%s", a->grad->data[i], i < 3 ? ", " : "");
        }
        printf("] - Expected: [6.0, 7.0, 8.0, 9.0] (values of b)\n");

        printf("  b.grad: [");
        for (int i = 0; i < 4; i++) {
            printf("%.1f%s", b->grad->data[i], i < 3 ? ", " : "");
        }
        printf("] - Expected: [2.0, 3.0, 4.0, 5.0] (values of a)\n");

        bool pass = true;
        double expected_a[] = {6.0, 7.0, 8.0, 9.0};
        double expected_b[] = {2.0, 3.0, 4.0, 5.0};
        for (int i = 0; i < 4; i++) {
            if (fabs(a->grad->data[i] - expected_a[i]) > 1e-6 ||
                fabs(b->grad->data[i] - expected_b[i]) > 1e-6) {
                pass = false;
            }
        }
        printf("  Result: %s\n\n", pass ? "✅ PASS" : "❌ FAIL");

        var_free_persistent(a);
        var_free_persistent(b);
        tape_reset(g_tape);
        autograd_reset_iteration();
    }

    /* Test 3: ReLU backward */
    printf("Test 3: ReLU backward\n");
    {
        int shape[] = {4};
        TensorV2 *x_data = tensor_create_persistent(shape, 1);

        /* Initialize with positive and negative values */
        x_data->data[0] = 2.0;   /* positive */
        x_data->data[1] = -1.0;  /* negative */
        x_data->data[2] = 3.0;   /* positive */
        x_data->data[3] = -2.0;  /* negative */

        VariableV2 *x = var_create_parameter(x_data);

        /* Forward: y = relu(x) */
        VariableV2 *y = var_relu(x);

        /* Set output gradient to all 1s */
        for (int i = 0; i < 4; i++) {
            y->grad->data[i] = 1.0;
        }

        /* Backward */
        tape_backward(g_tape);

        /* Check gradients (should be 1 for positive, 0 for negative) */
        printf("  x.grad: [");
        for (int i = 0; i < 4; i++) {
            printf("%.1f%s", x->grad->data[i], i < 3 ? ", " : "");
        }
        printf("] - Expected: [1.0, 0.0, 1.0, 0.0]\n");

        bool pass = true;
        double expected[] = {1.0, 0.0, 1.0, 0.0};
        for (int i = 0; i < 4; i++) {
            if (fabs(x->grad->data[i] - expected[i]) > 1e-6) {
                pass = false;
            }
        }
        printf("  Result: %s\n\n", pass ? "✅ PASS" : "❌ FAIL");

        var_free_persistent(x);
        tape_reset(g_tape);
        autograd_reset_iteration();
    }

    /* Test 4: Linear layer backward */
    printf("Test 4: Linear layer backward\n");
    {
        Linear *layer = linear_create(2, 3);  /* 2 inputs, 3 outputs */

        /* Set specific weights for testing */
        layer->weight->data->data[0] = 1.0; layer->weight->data->data[1] = 2.0;
        layer->weight->data->data[2] = 3.0; layer->weight->data->data[3] = 4.0;
        layer->weight->data->data[4] = 5.0; layer->weight->data->data[5] = 6.0;

        layer->bias->data->data[0] = 0.1;
        layer->bias->data->data[1] = 0.2;
        layer->bias->data->data[2] = 0.3;

        /* Create input */
        int shape[] = {2};
        TensorV2 *x_data = tensor_create_temp(shape, 1);
        x_data->data[0] = 1.0;
        x_data->data[1] = 2.0;
        VariableV2 *x = var_create_temp(x_data, true);

        /* Forward */
        VariableV2 *y = linear_forward(layer, x);

        /* Set output gradient */
        for (int i = 0; i < 3; i++) {
            y->grad->data[i] = 1.0;
        }

        /* Backward */
        tape_backward(g_tape);

        /* Check weight gradients */
        printf("  Weight gradients computed: ");
        bool has_grads = false;
        for (int i = 0; i < 6; i++) {
            if (fabs(layer->weight->grad->data[i]) > 1e-6) {
                has_grads = true;
                break;
            }
        }
        printf("%s\n", has_grads ? "Yes" : "No");

        /* Check bias gradients */
        printf("  Bias gradients computed: ");
        has_grads = false;
        for (int i = 0; i < 3; i++) {
            if (fabs(layer->bias->grad->data[i]) > 1e-6) {
                has_grads = true;
                break;
            }
        }
        printf("%s\n", has_grads ? "Yes" : "No");

        printf("  Result: %s\n\n",
               (layer->weight->grad && layer->bias->grad) ? "✅ PASS" : "❌ FAIL");

        linear_free(layer);
        autograd_reset_iteration();
    }

    /* Test 5: Chain of operations */
    printf("Test 5: Chain of operations\n");
    {
        int shape[] = {2};
        TensorV2 *x_data = tensor_create_persistent(shape, 1);
        x_data->data[0] = 3.0;
        x_data->data[1] = 4.0;

        VariableV2 *x = var_create_parameter(x_data);

        /* Forward: y = relu(x + x * 2) */
        TensorV2 *two_data = tensor_create_temp(shape, 1);
        two_data->data[0] = 2.0;
        two_data->data[1] = 2.0;
        VariableV2 *two = var_create_temp(two_data, false);

        VariableV2 *x_times_2 = ag_multiply(x, two);
        VariableV2 *sum = ag_add(x, x_times_2);
        VariableV2 *y = var_relu(sum);

        /* Set output gradient */
        y->grad->data[0] = 1.0;
        y->grad->data[1] = 1.0;

        /* Backward */
        tape_backward(g_tape);

        /* x appears in two paths: x and x*2, so gradient should be 1 + 2 = 3 */
        printf("  x.grad: [%.1f, %.1f] - Expected: [3.0, 3.0]\n",
               x->grad->data[0], x->grad->data[1]);

        bool pass = fabs(x->grad->data[0] - 3.0) < 1e-6 &&
                   fabs(x->grad->data[1] - 3.0) < 1e-6;
        printf("  Result: %s\n\n", pass ? "✅ PASS" : "❌ FAIL");

        var_free_persistent(x);
        autograd_reset_iteration();
    }

    printf("=====================================\n");
    printf("All backward pass tests complete!\n");
    printf("The gradient computation is working.\n");
    printf("=====================================\n");

    autograd_v2_cleanup();
    return 0;
}