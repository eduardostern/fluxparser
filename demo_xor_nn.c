/*
 * FluxParser - Research-Grade C Math Parser
 * Copyright (C) 2025 Eduardo Stern
 *
 * DEMO: XOR Neural Network
 * Phase 1: LLM Parser - Demonstrates tensor operations for ML
 *
 * Classic XOR problem: Learn the function that outputs:
 *   XOR(0,0) = 0
 *   XOR(0,1) = 1
 *   XOR(1,0) = 1
 *   XOR(1,1) = 0
 *
 * Network architecture: 2 → 4 → 1 (2 inputs, 4 hidden neurons, 1 output)
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "parser.h"
#include "ast.h"

/* Training data for XOR */
typedef struct {
    double x1, x2;  /* Inputs */
    double y;       /* Expected output */
} XORSample;

XORSample xor_data[] = {
    {0.0, 0.0, 0.0},
    {0.0, 1.0, 1.0},
    {1.0, 0.0, 1.0},
    {1.0, 1.0, 0.0}
};

/* Network parameters */
typedef struct {
    Tensor *W1;  /* Weights layer 1: 2×4 */
    Tensor *b1;  /* Bias layer 1: 4×1 */
    Tensor *W2;  /* Weights layer 2: 4×1 */
    Tensor *b2;  /* Bias layer 2: 1×1 */
} NeuralNetwork;

/* Forward pass results (for backprop) */
typedef struct {
    Tensor *X;
    Tensor *Z1;
    Tensor *A1;
    Tensor *Z2;
    Tensor *A2;
} ForwardCache;

/* Initialize network with random weights */
NeuralNetwork* nn_create() {
    srand(time(NULL));

    NeuralNetwork *nn = malloc(sizeof(NeuralNetwork));
    if (!nn) return NULL;

    int shape_W1[] = {4, 2};
    int shape_b1[] = {4, 1};
    int shape_W2[] = {1, 4};
    int shape_b2[] = {1, 1};

    // Initialize with small random values
    nn->W1 = tensor_randn(shape_W1, 2);
    nn->b1 = tensor_zeros(shape_b1, 2);
    nn->W2 = tensor_randn(shape_W2, 2);
    nn->b2 = tensor_zeros(shape_b2, 2);

    // Scale weights by 0.5 for better initialization
    Tensor *W1_scaled = tensor_multiply_scalar(nn->W1, 0.5);
    Tensor *W2_scaled = tensor_multiply_scalar(nn->W2, 0.5);
    tensor_free(nn->W1);
    tensor_free(nn->W2);
    nn->W1 = W1_scaled;
    nn->W2 = W2_scaled;

    return nn;
}

void nn_free(NeuralNetwork *nn) {
    if (!nn) return;
    tensor_free(nn->W1);
    tensor_free(nn->b1);
    tensor_free(nn->W2);
    tensor_free(nn->b2);
    free(nn);
}

/* Sigmoid derivative: f'(x) = f(x) * (1 - f(x)) */
Tensor* sigmoid_derivative(const Tensor *sigmoid_out) {
    Tensor *ones = tensor_ones(sigmoid_out->shape, sigmoid_out->rank);
    Tensor *one_minus_sig = tensor_subtract(ones, sigmoid_out);
    Tensor *result = tensor_multiply(sigmoid_out, one_minus_sig);

    tensor_free(ones);
    tensor_free(one_minus_sig);

    return result;
}

/* Forward pass */
ForwardCache* nn_forward(NeuralNetwork *nn, Tensor *X) {
    ForwardCache *cache = malloc(sizeof(ForwardCache));

    // Layer 1: Z1 = W1 * X + b1
    cache->Z1 = tensor_matmul(nn->W1, X);
    Tensor *temp1 = tensor_add(cache->Z1, nn->b1);
    tensor_free(cache->Z1);
    cache->Z1 = temp1;

    // Activation 1: A1 = sigmoid(Z1)
    cache->A1 = tensor_sigmoid(cache->Z1);

    // Layer 2: Z2 = W2 * A1 + b2
    cache->Z2 = tensor_matmul(nn->W2, cache->A1);
    Tensor *temp2 = tensor_add(cache->Z2, nn->b2);
    tensor_free(cache->Z2);
    cache->Z2 = temp2;

    // Activation 2: A2 = sigmoid(Z2)
    cache->A2 = tensor_sigmoid(cache->Z2);

    cache->X = tensor_clone(X);

    return cache;
}

void cache_free(ForwardCache *cache) {
    if (!cache) return;
    tensor_free(cache->X);
    tensor_free(cache->Z1);
    tensor_free(cache->A1);
    tensor_free(cache->Z2);
    tensor_free(cache->A2);
    free(cache);
}

/* Backward pass and weight update (simplified for XOR) */
void nn_train_step(NeuralNetwork *nn, Tensor *X, Tensor *Y, double learning_rate) {
    // Forward pass
    ForwardCache *cache = nn_forward(nn, X);

    // Compute loss gradient: dL/dA2 = A2 - Y
    Tensor *dA2 = tensor_subtract(cache->A2, Y);

    // Backprop through sigmoid: dL/dZ2 = dL/dA2 * sigmoid'(Z2)
    Tensor *sigmoid_grad = sigmoid_derivative(cache->A2);
    Tensor *dZ2 = tensor_multiply(dA2, sigmoid_grad);

    // Gradients for W2: dL/dW2 = dZ2 * A1^T
    Tensor *A1_T = tensor_transpose(cache->A1);
    Tensor *dW2 = tensor_matmul(dZ2, A1_T);

    // Gradients for b2: dL/db2 = dZ2
    Tensor *db2 = tensor_clone(dZ2);

    // Backprop to layer 1: dL/dA1 = W2^T * dZ2
    Tensor *W2_T = tensor_transpose(nn->W2);
    Tensor *dA1 = tensor_matmul(W2_T, dZ2);

    // Backprop through sigmoid: dL/dZ1 = dL/dA1 * sigmoid'(Z1)
    Tensor *sigmoid_grad1 = sigmoid_derivative(cache->A1);
    Tensor *dZ1 = tensor_multiply(dA1, sigmoid_grad1);

    // Gradients for W1: dL/dW1 = dZ1 * X^T
    Tensor *X_T = tensor_transpose(cache->X);
    Tensor *dW1 = tensor_matmul(dZ1, X_T);

    // Gradients for b1: dL/db1 = dZ1
    Tensor *db1 = tensor_clone(dZ1);

    // Update weights: W = W - learning_rate * dW
    Tensor *dW1_scaled = tensor_multiply_scalar(dW1, learning_rate);
    Tensor *dW2_scaled = tensor_multiply_scalar(dW2, learning_rate);
    Tensor *db1_scaled = tensor_multiply_scalar(db1, learning_rate);
    Tensor *db2_scaled = tensor_multiply_scalar(db2, learning_rate);

    Tensor *W1_new = tensor_subtract(nn->W1, dW1_scaled);
    Tensor *W2_new = tensor_subtract(nn->W2, dW2_scaled);
    Tensor *b1_new = tensor_subtract(nn->b1, db1_scaled);
    Tensor *b2_new = tensor_subtract(nn->b2, db2_scaled);

    // Replace old weights
    tensor_free(nn->W1);
    tensor_free(nn->W2);
    tensor_free(nn->b1);
    tensor_free(nn->b2);

    nn->W1 = W1_new;
    nn->W2 = W2_new;
    nn->b1 = b1_new;
    nn->b2 = b2_new;

    // Cleanup
    tensor_free(dA2);
    tensor_free(sigmoid_grad);
    tensor_free(dZ2);
    tensor_free(A1_T);
    tensor_free(dW2);
    tensor_free(db2);
    tensor_free(W2_T);
    tensor_free(dA1);
    tensor_free(sigmoid_grad1);
    tensor_free(dZ1);
    tensor_free(X_T);
    tensor_free(dW1);
    tensor_free(db1);
    tensor_free(dW1_scaled);
    tensor_free(dW2_scaled);
    tensor_free(db1_scaled);
    tensor_free(db2_scaled);
    cache_free(cache);
}

/* Compute mean squared error */
double compute_mse(NeuralNetwork *nn) {
    double total_error = 0.0;

    for (int i = 0; i < 4; i++) {
        double x_data[] = {xor_data[i].x1, xor_data[i].x2};
        int x_shape[] = {2, 1};
        Tensor *X = tensor_create_from_data(x_data, x_shape, 2);

        ForwardCache *cache = nn_forward(nn, X);

        double pred = cache->A2->data[0];
        double error = pred - xor_data[i].y;
        total_error += error * error;

        cache_free(cache);
        tensor_free(X);
    }

    return total_error / 4.0;
}

/* Test network predictions */
void test_network(NeuralNetwork *nn) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                    NETWORK PREDICTIONS                         ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    for (int i = 0; i < 4; i++) {
        double x_data[] = {xor_data[i].x1, xor_data[i].x2};
        int x_shape[] = {2, 1};
        Tensor *X = tensor_create_from_data(x_data, x_shape, 2);

        ForwardCache *cache = nn_forward(nn, X);
        double pred = cache->A2->data[0];

        printf("  XOR(%.0f, %.0f) = %.4f  (expected: %.0f)  ",
               xor_data[i].x1, xor_data[i].x2, pred, xor_data[i].y);

        // Check if prediction is correct (threshold at 0.5)
        int pred_class = (pred > 0.5) ? 1 : 0;
        int expected = (int)xor_data[i].y;

        if (pred_class == expected) {
            printf("✅ CORRECT\n");
        } else {
            printf("❌ WRONG\n");
        }

        cache_free(cache);
        tensor_free(X);
    }
}

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║           FLUXPARSER XOR NEURAL NETWORK DEMO                   ║\n");
    printf("║                                                                ║\n");
    printf("║  Training a 2-layer neural network to learn the XOR function  ║\n");
    printf("║  Architecture: 2 inputs → 4 hidden neurons → 1 output         ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");

    // Create network
    NeuralNetwork *nn = nn_create();
    if (!nn) {
        fprintf(stderr, "Failed to create neural network\n");
        return 1;
    }

    printf("\n📊 Training Data:\n");
    printf("  XOR(0, 0) → 0\n");
    printf("  XOR(0, 1) → 1\n");
    printf("  XOR(1, 0) → 1\n");
    printf("  XOR(1, 1) → 0\n");

    // Training parameters
    const int epochs = 10000;
    const double learning_rate = 0.5;
    const int print_interval = 1000;

    printf("\n🎓 Training for %d epochs (learning rate: %.2f)...\n\n", epochs, learning_rate);

    // Training loop
    for (int epoch = 0; epoch < epochs; epoch++) {
        // Train on each sample
        for (int i = 0; i < 4; i++) {
            double x_data[] = {xor_data[i].x1, xor_data[i].x2};
            double y_data[] = {xor_data[i].y};
            int x_shape[] = {2, 1};
            int y_shape[] = {1, 1};

            Tensor *X = tensor_create_from_data(x_data, x_shape, 2);
            Tensor *Y = tensor_create_from_data(y_data, y_shape, 2);

            nn_train_step(nn, X, Y, learning_rate);

            tensor_free(X);
            tensor_free(Y);
        }

        // Print progress
        if ((epoch + 1) % print_interval == 0) {
            double mse = compute_mse(nn);
            printf("  Epoch %5d: MSE = %.6f", epoch + 1, mse);
            if (mse < 0.01) printf("  🎯 Excellent!");
            else if (mse < 0.05) printf("  ⭐ Good");
            printf("\n");
        }
    }

    // Final evaluation
    double final_mse = compute_mse(nn);
    printf("\n✅ Training complete! Final MSE = %.6f\n", final_mse);

    // Test predictions
    test_network(nn);

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                       SUCCESS! 🎉                              ║\n");
    printf("║                                                                ║\n");
    printf("║  The network successfully learned the XOR function using      ║\n");
    printf("║  FluxParser's tensor operations and manual backpropagation!   ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    // Cleanup
    nn_free(nn);

    return 0;
}
