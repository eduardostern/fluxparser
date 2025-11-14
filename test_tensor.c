/*
 * FluxParser - Research-Grade C Math Parser
 * Copyright (C) 2025 Eduardo Stern
 *
 * TENSOR OPERATIONS TEST SUITE
 * Phase 1: LLM Parser - Matrix/Tensor Support
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "parser.h"
#include "ast.h"

#define TOLERANCE 1e-6

int tests_passed = 0;
int tests_failed = 0;

void test_passed(const char *name) {
    printf("  ✅ PASSED: %s\n", name);
    tests_passed++;
}

void test_failed(const char *name) {
    printf("  ❌ FAILED: %s\n", name);
    tests_failed++;
}

/* ============================================================================
 * TENSOR CREATION TESTS
 * ============================================================================ */

int test_tensor_creation() {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║ TENSOR CREATION                                                ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    // Test 1: Create 1D tensor (vector)
    int shape1[] = {5};
    Tensor *v = tensor_zeros(shape1, 1);
    assert(v != NULL);
    assert(v->rank == 1);
    assert(v->shape[0] == 5);
    assert(v->size == 5);
    tensor_free(v);
    test_passed("1D tensor creation");

    // Test 2: Create 2D tensor (matrix)
    int shape2[] = {3, 4};
    Tensor *m = tensor_ones(shape2, 2);
    assert(m != NULL);
    assert(m->rank == 2);
    assert(m->shape[0] == 3);
    assert(m->shape[1] == 4);
    assert(m->size == 12);
    assert(fabs(m->data[0] - 1.0) < TOLERANCE);
    tensor_free(m);
    test_passed("2D tensor creation");

    // Test 3: Create from data
    double data[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    int shape3[] = {2, 3};
    Tensor *t = tensor_create_from_data(data, shape3, 2);
    assert(t != NULL);
    assert(fabs(t->data[0] - 1.0) < TOLERANCE);
    assert(fabs(t->data[5] - 6.0) < TOLERANCE);
    tensor_free(t);
    test_passed("Tensor from data");

    // Test 4: Random tensors
    Tensor *r = tensor_random(shape1, 1);
    assert(r != NULL);
    assert(r->data[0] >= 0.0 && r->data[0] <= 1.0);
    tensor_free(r);
    test_passed("Random tensor");

    return 1;
}

/* ============================================================================
 * ELEMENT-WISE OPERATIONS TESTS
 * ============================================================================ */

int test_element_wise() {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║ ELEMENT-WISE OPERATIONS                                        ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    double data_a[] = {1.0, 2.0, 3.0, 4.0};
    double data_b[] = {5.0, 6.0, 7.0, 8.0};
    int shape[] = {2, 2};

    Tensor *a = tensor_create_from_data(data_a, shape, 2);
    Tensor *b = tensor_create_from_data(data_b, shape, 2);

    // Test 1: Addition
    Tensor *sum = tensor_add(a, b);
    assert(sum != NULL);
    assert(fabs(sum->data[0] - 6.0) < TOLERANCE);   // 1+5
    assert(fabs(sum->data[3] - 12.0) < TOLERANCE);  // 4+8
    tensor_free(sum);
    test_passed("Element-wise addition");

    // Test 2: Subtraction
    Tensor *diff = tensor_subtract(a, b);
    assert(diff != NULL);
    assert(fabs(diff->data[0] - (-4.0)) < TOLERANCE);  // 1-5
    assert(fabs(diff->data[3] - (-4.0)) < TOLERANCE);  // 4-8
    tensor_free(diff);
    test_passed("Element-wise subtraction");

    // Test 3: Multiplication (Hadamard)
    Tensor *prod = tensor_multiply(a, b);
    assert(prod != NULL);
    assert(fabs(prod->data[0] - 5.0) < TOLERANCE);   // 1*5
    assert(fabs(prod->data[3] - 32.0) < TOLERANCE);  // 4*8
    tensor_free(prod);
    test_passed("Element-wise multiplication");

    // Test 4: Scalar operations
    Tensor *scaled = tensor_multiply_scalar(a, 2.0);
    assert(scaled != NULL);
    assert(fabs(scaled->data[0] - 2.0) < TOLERANCE);  // 1*2
    assert(fabs(scaled->data[3] - 8.0) < TOLERANCE);  // 4*2
    tensor_free(scaled);
    test_passed("Scalar multiplication");

    tensor_free(a);
    tensor_free(b);

    return 1;
}

/* ============================================================================
 * MATRIX OPERATIONS TESTS
 * ============================================================================ */

int test_matrix_ops() {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║ MATRIX OPERATIONS                                              ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    // Test 1: Matrix multiplication (2x2 * 2x2)
    double data_a[] = {1.0, 2.0, 3.0, 4.0};
    double data_b[] = {5.0, 6.0, 7.0, 8.0};
    int shape[] = {2, 2};

    Tensor *a = tensor_create_from_data(data_a, shape, 2);
    Tensor *b = tensor_create_from_data(data_b, shape, 2);

    Tensor *c = tensor_matmul(a, b);
    assert(c != NULL);
    // C[0,0] = 1*5 + 2*7 = 19
    // C[0,1] = 1*6 + 2*8 = 22
    // C[1,0] = 3*5 + 4*7 = 43
    // C[1,1] = 3*6 + 4*8 = 50
    assert(fabs(c->data[0] - 19.0) < TOLERANCE);
    assert(fabs(c->data[1] - 22.0) < TOLERANCE);
    assert(fabs(c->data[2] - 43.0) < TOLERANCE);
    assert(fabs(c->data[3] - 50.0) < TOLERANCE);
    tensor_free(c);
    test_passed("Matrix multiplication 2x2");

    // Test 2: Matrix multiplication (2x3 * 3x2)
    double data_m[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    double data_n[] = {7.0, 8.0, 9.0, 10.0, 11.0, 12.0};
    int shape_m[] = {2, 3};
    int shape_n[] = {3, 2};

    Tensor *m = tensor_create_from_data(data_m, shape_m, 2);
    Tensor *n = tensor_create_from_data(data_n, shape_n, 2);

    Tensor *p = tensor_matmul(m, n);
    assert(p != NULL);
    assert(p->shape[0] == 2);
    assert(p->shape[1] == 2);
    // P[0,0] = 1*7 + 2*9 + 3*11 = 58
    assert(fabs(p->data[0] - 58.0) < TOLERANCE);
    tensor_free(p);
    tensor_free(m);
    tensor_free(n);
    test_passed("Matrix multiplication 2x3 * 3x2");

    // Test 3: Transpose
    Tensor *at = tensor_transpose(a);
    assert(at != NULL);
    assert(at->shape[0] == 2);
    assert(at->shape[1] == 2);
    assert(fabs(at->data[0] - 1.0) < TOLERANCE);  // A[0,0] → A^T[0,0]
    assert(fabs(at->data[1] - 3.0) < TOLERANCE);  // A[1,0] → A^T[0,1]
    assert(fabs(at->data[2] - 2.0) < TOLERANCE);  // A[0,1] → A^T[1,0]
    assert(fabs(at->data[3] - 4.0) < TOLERANCE);  // A[1,1] → A^T[1,1]
    tensor_free(at);
    test_passed("Matrix transpose");

    tensor_free(a);
    tensor_free(b);

    return 1;
}

/* ============================================================================
 * ACTIVATION FUNCTIONS TESTS
 * ============================================================================ */

int test_activations() {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║ ACTIVATION FUNCTIONS                                           ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    double data[] = {-2.0, -1.0, 0.0, 1.0, 2.0};
    int shape[] = {5};
    Tensor *x = tensor_create_from_data(data, shape, 1);

    // Test 1: ReLU
    Tensor *relu_out = tensor_relu(x);
    assert(relu_out != NULL);
    assert(fabs(relu_out->data[0] - 0.0) < TOLERANCE);  // max(0, -2)
    assert(fabs(relu_out->data[2] - 0.0) < TOLERANCE);  // max(0, 0)
    assert(fabs(relu_out->data[4] - 2.0) < TOLERANCE);  // max(0, 2)
    tensor_free(relu_out);
    test_passed("ReLU activation");

    // Test 2: Sigmoid
    Tensor *sig_out = tensor_sigmoid(x);
    assert(sig_out != NULL);
    // sigmoid(0) = 0.5
    assert(fabs(sig_out->data[2] - 0.5) < TOLERANCE);
    // sigmoid(x) should be in (0,1)
    for (int i = 0; i < 5; i++) {
        assert(sig_out->data[i] > 0.0 && sig_out->data[i] < 1.0);
    }
    tensor_free(sig_out);
    test_passed("Sigmoid activation");

    // Test 3: Tanh
    Tensor *tanh_out = tensor_tanh(x);
    assert(tanh_out != NULL);
    assert(fabs(tanh_out->data[2] - 0.0) < TOLERANCE);  // tanh(0) = 0
    // tanh(x) should be in (-1,1)
    for (int i = 0; i < 5; i++) {
        assert(tanh_out->data[i] > -1.0 && tanh_out->data[i] < 1.0);
    }
    tensor_free(tanh_out);
    test_passed("Tanh activation");

    // Test 4: Softmax
    double data2[] = {1.0, 2.0, 3.0};
    int shape2[] = {3};
    Tensor *x2 = tensor_create_from_data(data2, shape2, 1);
    Tensor *soft_out = tensor_softmax(x2);
    assert(soft_out != NULL);
    // Sum should be 1.0
    double sum = tensor_sum(soft_out);
    assert(fabs(sum - 1.0) < TOLERANCE);
    // All values should be positive
    for (int i = 0; i < 3; i++) {
        assert(soft_out->data[i] > 0.0);
    }
    tensor_free(soft_out);
    tensor_free(x2);
    test_passed("Softmax activation");

    tensor_free(x);

    return 1;
}

/* ============================================================================
 * REDUCTION OPERATIONS TESTS
 * ============================================================================ */

int test_reductions() {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║ REDUCTION OPERATIONS                                           ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    double data[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    int shape[] = {5};
    Tensor *x = tensor_create_from_data(data, shape, 1);

    // Test 1: Sum
    double sum = tensor_sum(x);
    assert(fabs(sum - 15.0) < TOLERANCE);  // 1+2+3+4+5
    test_passed("Tensor sum");

    // Test 2: Mean
    double mean = tensor_mean(x);
    assert(fabs(mean - 3.0) < TOLERANCE);  // 15/5
    test_passed("Tensor mean");

    // Test 3: Max
    double max = tensor_max(x);
    assert(fabs(max - 5.0) < TOLERANCE);
    test_passed("Tensor max");

    // Test 4: Min
    double min = tensor_min(x);
    assert(fabs(min - 1.0) < TOLERANCE);
    test_passed("Tensor min");

    tensor_free(x);

    return 1;
}

/* ============================================================================
 * NEURAL NETWORK LAYER TEST
 * ============================================================================ */

int test_nn_layer() {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║ NEURAL NETWORK LAYER (Forward Pass)                           ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    printf("Testing: Single layer forward pass Y = ReLU(W*X + b)\n\n");

    // Input: X = [1, 2] (2x1 vector)
    double x_data[] = {1.0, 2.0};
    int x_shape[] = {2, 1};
    Tensor *X = tensor_create_from_data(x_data, x_shape, 2);

    // Weights: W = [[0.5, 0.3], [0.2, 0.7]] (2x2 matrix)
    double w_data[] = {0.5, 0.3, 0.2, 0.7};
    int w_shape[] = {2, 2};
    Tensor *W = tensor_create_from_data(w_data, w_shape, 2);

    // Bias: b = [0.1, 0.2] (2x1 vector)
    double b_data[] = {0.1, 0.2};
    int b_shape[] = {2, 1};
    Tensor *b = tensor_create_from_data(b_data, b_shape, 2);

    // Forward pass: Z = W * X
    Tensor *Z = tensor_matmul(W, X);
    assert(Z != NULL);
    printf("  W * X = [%.4f, %.4f]^T\n", Z->data[0], Z->data[1]);

    // Z + b
    Tensor *Z_plus_b = tensor_add(Z, b);
    assert(Z_plus_b != NULL);
    printf("  Z + b = [%.4f, %.4f]^T\n", Z_plus_b->data[0], Z_plus_b->data[1]);

    // Y = ReLU(Z + b)
    Tensor *Y = tensor_relu(Z_plus_b);
    assert(Y != NULL);
    printf("  Y = ReLU(Z + b) = [%.4f, %.4f]^T\n", Y->data[0], Y->data[1]);

    // Verify results
    // W*X = [0.5*1 + 0.3*2, 0.2*1 + 0.7*2] = [1.1, 1.6]
    // Z+b = [1.2, 1.8]
    // ReLU([1.2, 1.8]) = [1.2, 1.8]
    assert(fabs(Y->data[0] - 1.2) < TOLERANCE);
    assert(fabs(Y->data[1] - 1.8) < TOLERANCE);

    tensor_free(X);
    tensor_free(W);
    tensor_free(b);
    tensor_free(Z);
    tensor_free(Z_plus_b);
    tensor_free(Y);

    test_passed("Neural network layer forward pass");

    return 1;
}

/* ============================================================================
 * MAIN TEST RUNNER
 * ============================================================================ */

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║              FLUXPARSER TENSOR OPERATIONS                      ║\n");
    printf("║                    Test Suite v1.0                             ║\n");
    printf("║                   Phase 1: LLM Parser                          ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");

    test_tensor_creation();
    test_element_wise();
    test_matrix_ops();
    test_activations();
    test_reductions();
    test_nn_layer();

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                        TEST SUMMARY                            ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║  ✅ Passed: %-3d                                              ║\n", tests_passed);
    printf("║  ❌ Failed: %-3d                                              ║\n", tests_failed);
    printf("║  Total:    %-3d                                              ║\n", tests_passed + tests_failed);
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    return (tests_failed == 0) ? 0 : 1;
}
