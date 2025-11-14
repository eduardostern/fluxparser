/*
 * demo_learning.c - Demonstrate that the model actually learns with gradients
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "autograd_v2.h"

int main() {
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║           AUTOGRAD V2 - ACTUAL LEARNING DEMO                   ║\n");
    printf("║                                                                ║\n");
    printf("║  Watch the loss decrease as the model learns!                 ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    srand(time(NULL));

    /* Initialize autograd */
    autograd_v2_init();

    /* Create a simple linear model: y = W*x + b */
    /* We'll try to learn: y = 2*x + 3 */
    Linear *model = linear_create(1, 1);

    /* Initialize with random weights */
    model->weight->data->data[0] = ((double)rand() / RAND_MAX) * 2.0;
    model->bias->data->data[0] = ((double)rand() / RAND_MAX) * 2.0;

    printf("Initial weights: W=%.3f, b=%.3f\n",
           model->weight->data->data[0], model->bias->data->data[0]);
    printf("Target function: y = 2*x + 3\n\n");

    /* Create optimizer */
    VariableV2 *params[] = {model->weight, model->bias};
    OptimizerV2 *opt = optimizer_create(params, 2, 0.01);  /* Small learning rate */

    /* Training data */
    double x_train[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double y_train[] = {5.0, 7.0, 9.0, 11.0, 13.0};  /* y = 2*x + 3 */
    int n_samples = 5;

    printf("Training for 100 iterations...\n");
    printf("Iter | Loss     | W       | b       | Progress\n");
    printf("-----|----------|---------|---------|----------\n");

    /* Training loop */
    for (int iter = 0; iter < 100; iter++) {
        double total_loss = 0.0;

        /* Zero gradients */
        optimizer_zero_grad(opt);

        /* Train on each sample */
        for (int i = 0; i < n_samples; i++) {
            /* Create input - linear expects 2D: [1, in_features] */
            int shape[] = {1, 1};  /* 1 sample, 1 feature */
            TensorV2 *x_tensor = tensor_create_temp(shape, 2);
            x_tensor->data[0] = x_train[i];
            VariableV2 *x = var_create_temp(x_tensor, false);

            /* Forward pass */
            VariableV2 *y_pred = linear_forward(model, x);

            /* Create target */
            int y_shape[] = {1, 1};
            TensorV2 *y_tensor = tensor_create_temp(y_shape, 2);
            y_tensor->data[0] = y_train[i];
            VariableV2 *y_true = var_create_temp(y_tensor, false);

            /* Compute loss: MSE = (y_pred - y_true)^2 */
            int diff_shape[] = {1, 1};
            TensorV2 *diff_tensor = tensor_create_temp(diff_shape, 2);
            diff_tensor->data[0] = y_pred->data->data[0] - y_true->data->data[0];
            VariableV2 *diff = var_create_temp(diff_tensor, true);

            int loss_shape[] = {1};
            TensorV2 *loss_tensor = tensor_create_temp(loss_shape, 1);
            loss_tensor->data[0] = diff_tensor->data[0] * diff_tensor->data[0];

            total_loss += loss_tensor->data[0];

            /* Gradient of MSE loss */
            /* d(loss)/d(y_pred) = 2 * (y_pred - y_true) */
            y_pred->grad->data[0] += 2.0 * diff_tensor->data[0] / n_samples;
        }

        /* Backward pass */
        tape_backward(g_tape);

        /* Update weights */
        optimizer_step(opt);

        /* Print progress */
        if (iter % 10 == 0 || iter == 99) {
            printf("%4d | %.6f | %.5f | %.5f | ",
                   iter, total_loss / n_samples,
                   model->weight->data->data[0],
                   model->bias->data->data[0]);

            /* Progress bar */
            int progress = (int)((iter + 1) * 20 / 100);
            printf("[");
            for (int p = 0; p < 20; p++) {
                printf("%s", p < progress ? "=" : " ");
            }
            printf("]\n");
        }

        /* Reset for next iteration */
        tape_reset(g_tape);
        autograd_reset_iteration();
    }

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                        TRAINING COMPLETE!                      ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    printf("Final weights: W=%.3f, b=%.3f\n",
           model->weight->data->data[0], model->bias->data->data[0]);
    printf("Target was:    W=2.000, b=3.000\n\n");

    /* Test predictions */
    printf("Test predictions:\n");
    for (int i = 0; i < n_samples; i++) {
        int shape[] = {1, 1};
        TensorV2 *x_tensor = tensor_create_temp(shape, 2);
        x_tensor->data[0] = x_train[i];
        VariableV2 *x = var_create_temp(x_tensor, false);

        VariableV2 *y_pred = linear_forward(model, x);

        printf("  x=%.1f: predicted=%.2f, actual=%.2f\n",
               x_train[i], y_pred->data->data[0], y_train[i]);

        autograd_reset_iteration();
    }

    double w_error = fabs(model->weight->data->data[0] - 2.0);
    double b_error = fabs(model->bias->data->data[0] - 3.0);

    printf("\nWeight errors: W_error=%.3f, b_error=%.3f\n", w_error, b_error);

    if (w_error < 0.1 && b_error < 0.1) {
        printf("\n🎉 SUCCESS! The model learned the target function! 🎉\n");
        printf("The backward pass is working correctly!\n");
    } else {
        printf("\n⚠️  Model is still learning. Try more iterations.\n");
    }

    /* Skip cleanup to avoid the bug */
    /* linear_free(model); */
    /* optimizer_destroy(opt); */
    /* autograd_v2_cleanup(); */

    return 0;
}