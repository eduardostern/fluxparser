/*
 * test_cross_entropy.c - Test cross-entropy loss for language modeling
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "autograd_v2.h"

/* Softmax helper */
void compute_softmax(double *logits, double *probs, int size) {
    double max_logit = logits[0];
    for (int i = 1; i < size; i++) {
        if (logits[i] > max_logit) max_logit = logits[i];
    }

    double sum = 0.0;
    for (int i = 0; i < size; i++) {
        probs[i] = exp(logits[i] - max_logit);
        sum += probs[i];
    }

    for (int i = 0; i < size; i++) {
        probs[i] /= sum;
    }
}

/* Cross-entropy loss: -log(p[target]) */
double cross_entropy_loss(double *logits, int target, int vocab_size) {
    double *probs = malloc(vocab_size * sizeof(double));
    compute_softmax(logits, probs, vocab_size);
    double loss = -log(probs[target] + 1e-10);  /* Add small epsilon for stability */
    free(probs);
    return loss;
}

/* Cross-entropy backward: gradient is (softmax - one_hot) */
void cross_entropy_backward(double *logits, int target, double *grad, int vocab_size) {
    double *probs = malloc(vocab_size * sizeof(double));
    compute_softmax(logits, probs, vocab_size);

    for (int i = 0; i < vocab_size; i++) {
        grad[i] = probs[i];
    }
    grad[target] -= 1.0;  /* Subtract 1 at target position */

    free(probs);
}

int main() {
    printf("═══════════════════════════════════════════════\n");
    printf("       Cross-Entropy Loss for LLM Training     \n");
    printf("═══════════════════════════════════════════════\n\n");

    /* Initialize autograd */
    autograd_v2_init();

    /* Simulate a small vocabulary */
    int vocab_size = 5;
    int target = 2;  /* Target token is index 2 */

    printf("Vocabulary size: %d\n", vocab_size);
    printf("Target token: %d\n\n", target);

    /* Create logits (output from model) */
    int shape[] = {vocab_size};
    TensorV2 *logits_tensor = tensor_create_persistent(shape, 1);

    /* Initialize with some values */
    logits_tensor->data[0] = 2.0;
    logits_tensor->data[1] = 1.0;
    logits_tensor->data[2] = 3.0;  /* Highest logit at target */
    logits_tensor->data[3] = 0.5;
    logits_tensor->data[4] = 1.5;

    VariableV2 *logits = var_create_parameter(logits_tensor);

    printf("Logits: [");
    for (int i = 0; i < vocab_size; i++) {
        printf("%.2f%s", logits->data->data[i], i < vocab_size-1 ? ", " : "");
    }
    printf("]\n");

    /* Compute softmax probabilities */
    double *probs = malloc(vocab_size * sizeof(double));
    compute_softmax(logits->data->data, probs, vocab_size);

    printf("Softmax probs: [");
    for (int i = 0; i < vocab_size; i++) {
        printf("%.4f%s", probs[i], i < vocab_size-1 ? ", " : "");
    }
    printf("]\n");
    printf("Probability at target (%d): %.4f\n", target, probs[target]);

    /* Compute loss */
    double loss = cross_entropy_loss(logits->data->data, target, vocab_size);
    printf("\nCross-entropy loss: %.4f\n", loss);
    printf("(Lower is better, 0 = perfect prediction)\n");

    /* Compute gradients manually */
    printf("\n--- Gradient Computation ---\n");
    double *manual_grad = malloc(vocab_size * sizeof(double));
    cross_entropy_backward(logits->data->data, target, manual_grad, vocab_size);

    printf("Gradients (softmax - one_hot):\n");
    for (int i = 0; i < vocab_size; i++) {
        printf("  grad[%d] = %.4f", i, manual_grad[i]);
        if (i == target) {
            printf(" (target: %.4f - 1 = %.4f)", probs[i], manual_grad[i]);
        }
        printf("\n");
    }

    /* Key insight for LLM training */
    printf("\n═══════════════════════════════════════════════\n");
    printf("Key Insights for LLM Training:\n");
    printf("1. Loss measures -log(p) of correct next token\n");
    printf("2. Gradient is simply (predicted - actual)\n");
    printf("3. Model learns by pushing probability mass\n");
    printf("   toward the correct token\n");
    printf("4. This is how GPT models learn language!\n");
    printf("═══════════════════════════════════════════════\n");

    /* Simulate one gradient descent step */
    printf("\n--- After Gradient Step (lr=0.1) ---\n");
    double lr = 0.1;
    for (int i = 0; i < vocab_size; i++) {
        logits->data->data[i] -= lr * manual_grad[i];
    }

    printf("Updated logits: [");
    for (int i = 0; i < vocab_size; i++) {
        printf("%.2f%s", logits->data->data[i], i < vocab_size-1 ? ", " : "");
    }
    printf("]\n");

    /* Check new probabilities */
    compute_softmax(logits->data->data, probs, vocab_size);
    printf("New probability at target: %.4f (was %.4f)\n",
           probs[target], probs[target]);

    double new_loss = cross_entropy_loss(logits->data->data, target, vocab_size);
    printf("New loss: %.4f (was %.4f) - %s\n",
           new_loss, loss,
           new_loss < loss ? "✅ Improved!" : "❌ Worse");

    /* Cleanup */
    free(probs);
    free(manual_grad);
    var_free_persistent(logits);
    autograd_v2_cleanup();

    return 0;
}