/*
 * FluxParser - Research-Grade C Math Parser
 * Copyright (C) 2025 Eduardo Stern
 *
 * Dual Licensed:
 * - GPL-3.0 for open-source/non-commercial use
 * - Commercial license available - see LICENSE-COMMERCIAL.md
 *
 * SPDX-License-Identifier: GPL-3.0-or-later OR Commercial
 *
 * TENSOR OPERATIONS - Phase 1: LLM Parser
 * Implements multi-dimensional arrays for machine learning workloads
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "ast.h"
#include "arena.h"

/* ============================================================================
 * TENSOR CREATION & MANAGEMENT
 * ============================================================================ */

/* Create tensor with given shape */
Tensor* tensor_create(const int *shape, int rank) {
    if (rank < 0 || rank > 8) {
        fprintf(stderr, "Invalid tensor rank: %d (must be 0-8)\n", rank);
        return NULL;
    }

    Tensor *t = malloc(sizeof(Tensor));
    if (!t) return NULL;

    t->rank = rank;
    t->ref_count = 1;

    // Calculate total size
    t->size = 1;
    for (int i = 0; i < rank; i++) {
        if (shape[i] <= 0) {
            free(t);
            return NULL;
        }
        t->size *= shape[i];
    }

    // Allocate shape array
    t->shape = malloc(sizeof(int) * rank);
    if (!t->shape && rank > 0) {
        free(t);
        return NULL;
    }
    memcpy(t->shape, shape, sizeof(int) * rank);

    // Allocate data array (uninitialized)
    t->data = malloc(sizeof(double) * t->size);
    if (!t->data) {
        free(t->shape);
        free(t);
        return NULL;
    }

    return t;
}

/* Create tensor from existing data */
Tensor* tensor_create_from_data(const double *data, const int *shape, int rank) {
    Tensor *t = tensor_create(shape, rank);
    if (!t) return NULL;

    memcpy(t->data, data, sizeof(double) * t->size);
    return t;
}

/* Create tensor filled with zeros */
Tensor* tensor_zeros(const int *shape, int rank) {
    Tensor *t = tensor_create(shape, rank);
    if (!t) return NULL;

    for (int i = 0; i < t->size; i++) {
        t->data[i] = 0.0;
    }
    return t;
}

/* Create tensor filled with ones */
Tensor* tensor_ones(const int *shape, int rank) {
    Tensor *t = tensor_create(shape, rank);
    if (!t) return NULL;

    for (int i = 0; i < t->size; i++) {
        t->data[i] = 1.0;
    }
    return t;
}

/* Create tensor with random uniform [0,1] values */
Tensor* tensor_random(const int *shape, int rank) {
    static bool seeded = false;
    if (!seeded) {
        srand(time(NULL));
        seeded = true;
    }

    Tensor *t = tensor_create(shape, rank);
    if (!t) return NULL;

    for (int i = 0; i < t->size; i++) {
        t->data[i] = (double)rand() / RAND_MAX;
    }
    return t;
}

/* Create tensor with random normal N(0,1) values (Box-Muller transform) */
Tensor* tensor_randn(const int *shape, int rank) {
    static bool seeded = false;
    if (!seeded) {
        srand(time(NULL));
        seeded = true;
    }

    Tensor *t = tensor_create(shape, rank);
    if (!t) return NULL;

    for (int i = 0; i < t->size; i += 2) {
        // Box-Muller transform
        double u1 = (double)rand() / RAND_MAX;
        double u2 = (double)rand() / RAND_MAX;
        double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
        double z1 = sqrt(-2.0 * log(u1)) * sin(2.0 * M_PI * u2);

        t->data[i] = z0;
        if (i + 1 < t->size) {
            t->data[i + 1] = z1;
        }
    }
    return t;
}

/* Create arena-allocated tensor */
Tensor* tensor_create_temp(const int *shape, int rank) {
    if (rank < 0 || rank > 8) {
        fprintf(stderr, "Invalid tensor rank: %d (must be 0-8)\n", rank);
        return NULL;
    }

    if (!global_arena) {
        arena_init_global();
    }

    Tensor *t = arena_alloc(global_arena, sizeof(Tensor));
    if (!t) return NULL;

    t->rank = rank;
    t->ref_count = 1;

    // Calculate total size
    t->size = 1;
    for (int i = 0; i < rank; i++) {
        if (shape[i] <= 0) {
            return NULL;
        }
        t->size *= shape[i];
    }

    // Allocate shape array from arena
    t->shape = arena_alloc(global_arena, sizeof(int) * rank);
    if (!t->shape) return NULL;
    memcpy(t->shape, shape, sizeof(int) * rank);

    // Allocate data from arena
    t->data = arena_calloc(global_arena, t->size, sizeof(double));
    if (!t->data) return NULL;

    return t;
}

/* Free tensor */
void tensor_free(Tensor *tensor) {
    if (!tensor) return;

    /* Don't free arena-allocated tensors individually */
    if (tensor->data) free(tensor->data);
    if (tensor->shape) free(tensor->shape);
    free(tensor);
}

/* Clone tensor (deep copy) */
Tensor* tensor_clone(const Tensor *tensor) {
    if (!tensor) return NULL;

    return tensor_create_from_data(tensor->data, tensor->shape, tensor->rank);
}

/* Reference counting */
void tensor_retain(Tensor *tensor) {
    if (tensor) {
        tensor->ref_count++;
    }
}

void tensor_release(Tensor *tensor) {
    if (!tensor) return;

    tensor->ref_count--;
    if (tensor->ref_count <= 0) {
        tensor_free(tensor);
    }
}

/* ============================================================================
 * TENSOR PROPERTIES
 * ============================================================================ */

/* Print tensor */
void tensor_print(const Tensor *tensor) {
    if (!tensor) {
        printf("NULL tensor\n");
        return;
    }

    printf("Tensor(shape=[");
    for (int i = 0; i < tensor->rank; i++) {
        printf("%d", tensor->shape[i]);
        if (i < tensor->rank - 1) printf(", ");
    }
    printf("], size=%d):\n", tensor->size);

    // Print data (simplified - just show first/last elements if large)
    if (tensor->size <= 20) {
        for (int i = 0; i < tensor->size; i++) {
            printf("  [%d] = %.6f\n", i, tensor->data[i]);
        }
    } else {
        for (int i = 0; i < 5; i++) {
            printf("  [%d] = %.6f\n", i, tensor->data[i]);
        }
        printf("  ...\n");
        for (int i = tensor->size - 5; i < tensor->size; i++) {
            printf("  [%d] = %.6f\n", i, tensor->data[i]);
        }
    }
}

/* Get total size */
int tensor_get_size(const Tensor *tensor) {
    return tensor ? tensor->size : 0;
}

/* Check if two tensors have same shape */
bool tensor_same_shape(const Tensor *a, const Tensor *b) {
    if (!a || !b) return false;
    if (a->rank != b->rank) return false;

    for (int i = 0; i < a->rank; i++) {
        if (a->shape[i] != b->shape[i]) return false;
    }
    return true;
}

/* ============================================================================
 * ELEMENT-WISE OPERATIONS
 * ============================================================================ */

/* Element-wise addition */
Tensor* tensor_add(const Tensor *a, const Tensor *b) {
    if (!tensor_same_shape(a, b)) {
        fprintf(stderr, "Cannot add tensors with different shapes\n");
        return NULL;
    }

    Tensor *result = tensor_create(a->shape, a->rank);
    if (!result) return NULL;

    for (int i = 0; i < a->size; i++) {
        result->data[i] = a->data[i] + b->data[i];
    }
    return result;
}

/* Element-wise subtraction */
Tensor* tensor_subtract(const Tensor *a, const Tensor *b) {
    if (!tensor_same_shape(a, b)) {
        fprintf(stderr, "Cannot subtract tensors with different shapes\n");
        return NULL;
    }

    Tensor *result = tensor_create(a->shape, a->rank);
    if (!result) return NULL;

    for (int i = 0; i < a->size; i++) {
        result->data[i] = a->data[i] - b->data[i];
    }
    return result;
}

/* Element-wise multiplication (Hadamard product) */
Tensor* tensor_multiply(const Tensor *a, const Tensor *b) {
    if (!tensor_same_shape(a, b)) {
        fprintf(stderr, "Cannot multiply tensors with different shapes\n");
        return NULL;
    }

    Tensor *result = tensor_create(a->shape, a->rank);
    if (!result) return NULL;

    for (int i = 0; i < a->size; i++) {
        result->data[i] = a->data[i] * b->data[i];
    }
    return result;
}

/* Element-wise division */
Tensor* tensor_divide(const Tensor *a, const Tensor *b) {
    if (!tensor_same_shape(a, b)) {
        fprintf(stderr, "Cannot divide tensors with different shapes\n");
        return NULL;
    }

    Tensor *result = tensor_create(a->shape, a->rank);
    if (!result) return NULL;

    for (int i = 0; i < a->size; i++) {
        if (fabs(b->data[i]) < 1e-10) {
            fprintf(stderr, "Division by zero in tensor\n");
            tensor_free(result);
            return NULL;
        }
        result->data[i] = a->data[i] / b->data[i];
    }
    return result;
}

/* Element-wise negation */
Tensor* tensor_negate(const Tensor *a) {
    Tensor *result = tensor_create(a->shape, a->rank);
    if (!result) return NULL;

    for (int i = 0; i < a->size; i++) {
        result->data[i] = -a->data[i];
    }
    return result;
}

/* ============================================================================
 * SCALAR OPERATIONS
 * ============================================================================ */

/* Add scalar to all elements */
Tensor* tensor_add_scalar(const Tensor *a, double scalar) {
    Tensor *result = tensor_create(a->shape, a->rank);
    if (!result) return NULL;

    for (int i = 0; i < a->size; i++) {
        result->data[i] = a->data[i] + scalar;
    }
    return result;
}

/* Multiply all elements by scalar */
Tensor* tensor_multiply_scalar(const Tensor *a, double scalar) {
    Tensor *result = tensor_create(a->shape, a->rank);
    if (!result) return NULL;

    for (int i = 0; i < a->size; i++) {
        result->data[i] = a->data[i] * scalar;
    }
    return result;
}

/* ============================================================================
 * MATRIX OPERATIONS
 * ============================================================================ */

/* Matrix multiplication (2D tensors only) */
Tensor* tensor_matmul(const Tensor *a, const Tensor *b) {
    // Check dimensions
    if (a->rank != 2 || b->rank != 2) {
        fprintf(stderr, "matmul requires 2D tensors (matrices)\n");
        return NULL;
    }

    int m = a->shape[0];  // rows of A
    int n = a->shape[1];  // cols of A
    int p = b->shape[1];  // cols of B

    if (n != b->shape[0]) {
        fprintf(stderr, "matmul shape mismatch: (%d,%d) x (%d,%d)\n",
                m, n, b->shape[0], p);
        return NULL;
    }

    // Result is m x p
    int result_shape[2] = {m, p};
    Tensor *result = tensor_zeros(result_shape, 2);
    if (!result) return NULL;

    // Perform matrix multiplication: C[i,j] = Σ A[i,k] * B[k,j]
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < p; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) {
                sum += a->data[i * n + k] * b->data[k * p + j];
            }
            result->data[i * p + j] = sum;
        }
    }

    return result;
}

/* Transpose (2D tensors only) */
Tensor* tensor_transpose(const Tensor *a) {
    if (a->rank != 2) {
        fprintf(stderr, "transpose requires 2D tensor (matrix)\n");
        return NULL;
    }

    int m = a->shape[0];
    int n = a->shape[1];

    int result_shape[2] = {n, m};  // Swap dimensions
    Tensor *result = tensor_create(result_shape, 2);
    if (!result) return NULL;

    // A[i,j] → A^T[j,i]
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            result->data[j * m + i] = a->data[i * n + j];
        }
    }

    return result;
}

/* Dot product (1D tensors only) */
Tensor* tensor_dot(const Tensor *a, const Tensor *b) {
    if (a->rank != 1 || b->rank != 1) {
        fprintf(stderr, "dot product requires 1D tensors (vectors)\n");
        return NULL;
    }

    if (a->size != b->size) {
        fprintf(stderr, "dot product requires same-length vectors\n");
        return NULL;
    }

    double sum = 0.0;
    for (int i = 0; i < a->size; i++) {
        sum += a->data[i] * b->data[i];
    }

    // Return scalar as rank-0 tensor
    int shape[1] = {1};
    Tensor *result = tensor_create(shape, 0);
    result->data[0] = sum;
    return result;
}

/* ============================================================================
 * ACTIVATION FUNCTIONS
 * ============================================================================ */

/* ReLU: max(0, x) */
Tensor* tensor_relu(const Tensor *x) {
    Tensor *result = tensor_create(x->shape, x->rank);
    if (!result) return NULL;

    for (int i = 0; i < x->size; i++) {
        result->data[i] = (x->data[i] > 0.0) ? x->data[i] : 0.0;
    }
    return result;
}

/* Sigmoid: 1 / (1 + e^(-x)) */
Tensor* tensor_sigmoid(const Tensor *x) {
    Tensor *result = tensor_create(x->shape, x->rank);
    if (!result) return NULL;

    for (int i = 0; i < x->size; i++) {
        result->data[i] = 1.0 / (1.0 + exp(-x->data[i]));
    }
    return result;
}

/* Tanh: (e^x - e^(-x)) / (e^x + e^(-x)) */
Tensor* tensor_tanh(const Tensor *x) {
    Tensor *result = tensor_create(x->shape, x->rank);
    if (!result) return NULL;

    for (int i = 0; i < x->size; i++) {
        result->data[i] = tanh(x->data[i]);
    }
    return result;
}

/* Softmax: exp(x_i) / Σ exp(x_j) */
Tensor* tensor_softmax(const Tensor *x) {
    Tensor *result = tensor_create(x->shape, x->rank);
    if (!result) return NULL;

    // Find max for numerical stability
    double max_val = x->data[0];
    for (int i = 1; i < x->size; i++) {
        if (x->data[i] > max_val) max_val = x->data[i];
    }

    // Compute exp(x - max)
    double sum = 0.0;
    for (int i = 0; i < x->size; i++) {
        result->data[i] = exp(x->data[i] - max_val);
        sum += result->data[i];
    }

    // Normalize
    for (int i = 0; i < x->size; i++) {
        result->data[i] /= sum;
    }

    return result;
}

/* ============================================================================
 * REDUCTION OPERATIONS
 * ============================================================================ */

/* Sum all elements */
double tensor_sum(const Tensor *x) {
    double sum = 0.0;
    for (int i = 0; i < x->size; i++) {
        sum += x->data[i];
    }
    return sum;
}

/* Mean of all elements */
double tensor_mean(const Tensor *x) {
    if (x->size == 0) return 0.0;
    return tensor_sum(x) / x->size;
}

/* Maximum element */
double tensor_max(const Tensor *x) {
    if (x->size == 0) return 0.0;

    double max_val = x->data[0];
    for (int i = 1; i < x->size; i++) {
        if (x->data[i] > max_val) max_val = x->data[i];
    }
    return max_val;
}

/* Minimum element */
double tensor_min(const Tensor *x) {
    if (x->size == 0) return 0.0;

    double min_val = x->data[0];
    for (int i = 1; i < x->size; i++) {
        if (x->data[i] < min_val) min_val = x->data[i];
    }
    return min_val;
}

/* ============================================================================
 * AST INTEGRATION
 * ============================================================================ */

/* Create AST node from tensor */
ASTNode* ast_create_tensor(Tensor *tensor) {
    if (!tensor) return NULL;

    ASTNode *node = malloc(sizeof(ASTNode));
    if (!node) return NULL;

    node->type = AST_TENSOR;
    node->data.tensor.tensor = tensor;
    tensor_retain(tensor);  // Increment ref count

    return node;
}

/* AST tensor matrix multiplication */
ASTNode* ast_tensor_matmul(ASTNode *a, ASTNode *b) {
    if (!a || !b) return NULL;
    if (a->type != AST_TENSOR || b->type != AST_TENSOR) {
        fprintf(stderr, "matmul requires tensor nodes\n");
        return NULL;
    }

    Tensor *result = tensor_matmul(a->data.tensor.tensor, b->data.tensor.tensor);
    if (!result) return NULL;

    return ast_create_tensor(result);
}

/* AST tensor addition */
ASTNode* ast_tensor_add(ASTNode *a, ASTNode *b) {
    if (!a || !b) return NULL;
    if (a->type != AST_TENSOR || b->type != AST_TENSOR) {
        fprintf(stderr, "tensor add requires tensor nodes\n");
        return NULL;
    }

    Tensor *result = tensor_add(a->data.tensor.tensor, b->data.tensor.tensor);
    if (!result) return NULL;

    return ast_create_tensor(result);
}
