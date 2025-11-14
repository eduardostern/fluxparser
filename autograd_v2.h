/*
 * AUTOGRAD V2 - Complete Rewrite with Proper Memory Management
 *
 * Design Principles:
 * 1. ALL intermediate values use arena allocation
 * 2. Parameters use heap allocation (persistent)
 * 3. Clear ownership and lifecycle management
 * 4. Simple, clean API
 */

#ifndef AUTOGRAD_V2_H
#define AUTOGRAD_V2_H

#include <stdbool.h>
#include <stddef.h>

/* Forward declarations */
typedef struct TensorV2 TensorV2;
typedef struct VariableV2 VariableV2;
typedef struct TapeV2 TapeV2;
typedef struct OptimizerV2 OptimizerV2;

/* ============================================================================
 * TENSOR V2 - Simplified tensor with clear memory ownership
 * ============================================================================ */

typedef enum {
    TENSOR_PERSISTENT,  /* Heap allocated (for parameters) */
    TENSOR_TEMPORARY    /* Arena allocated (for intermediates) */
} TensorStorage;

struct TensorV2 {
    double *data;
    int *shape;
    int rank;
    int size;
    TensorStorage storage;
};

/* Tensor creation */
TensorV2* tensor_create_persistent(const int *shape, int rank);
TensorV2* tensor_create_temp(const int *shape, int rank);
TensorV2* tensor_zeros_persistent(const int *shape, int rank);
TensorV2* tensor_zeros_temp(const int *shape, int rank);
TensorV2* tensor_randn_persistent(const int *shape, int rank, double scale);
TensorV2* tensor_clone_temp(const TensorV2 *src);
void tensor_free_persistent(TensorV2 *t);
/* No free for temp tensors - arena handles it */

/* Tensor operations (all return temp tensors) */
TensorV2* tensor_add(const TensorV2 *a, const TensorV2 *b);
TensorV2* tensor_subtract(const TensorV2 *a, const TensorV2 *b);
TensorV2* tensor_multiply(const TensorV2 *a, const TensorV2 *b);
TensorV2* tensor_matmul(const TensorV2 *a, const TensorV2 *b);
TensorV2* tensor_transpose(const TensorV2 *a);
TensorV2* tensor_relu(const TensorV2 *x);
TensorV2* tensor_softmax(const TensorV2 *x);
double tensor_sum(const TensorV2 *x);

/* ============================================================================
 * VARIABLE V2 - Simplified variable with automatic differentiation
 * ============================================================================ */

struct VariableV2 {
    TensorV2 *data;
    TensorV2 *grad;
    bool requires_grad;
    bool is_parameter;  /* If true, don't free during arena reset */
};

/* Variable creation */
VariableV2* var_create_parameter(TensorV2 *data);  /* For model parameters */
VariableV2* var_create_temp(TensorV2 *data, bool requires_grad);  /* For intermediates */
void var_zero_grad(VariableV2 *var);

/* ============================================================================
 * TAPE V2 - Simplified computation graph
 * ============================================================================ */

typedef void (*BackwardFunc)(void *ctx, TensorV2 *grad_output);

typedef struct TapeOp {
    VariableV2 **inputs;
    int num_inputs;
    VariableV2 *output;
    BackwardFunc backward;
    void *ctx;
} TapeOp;

struct TapeV2 {
    TapeOp *ops;
    int count;
    int capacity;
};

/* Tape operations */
TapeV2* tape_create(void);
void tape_destroy(TapeV2 *tape);
void tape_reset(TapeV2 *tape);
void tape_add_op(TapeV2 *tape, VariableV2 **inputs, int num_inputs,
                 VariableV2 *output, BackwardFunc backward, void *ctx);
void tape_backward(TapeV2 *tape);

/* ============================================================================
 * AUTOGRAD OPERATIONS - All return temporary Variables
 * ============================================================================ */

VariableV2* ag_add(VariableV2 *a, VariableV2 *b);
VariableV2* ag_subtract(VariableV2 *a, VariableV2 *b);
VariableV2* ag_multiply(VariableV2 *a, VariableV2 *b);
VariableV2* ag_matmul(VariableV2 *a, VariableV2 *b);
VariableV2* ag_relu(VariableV2 *x);
VariableV2* ag_softmax(VariableV2 *x);
VariableV2* ag_transpose(VariableV2 *x);
VariableV2* ag_cross_entropy_loss(VariableV2 *logits, VariableV2 *targets);

/* ============================================================================
 * LAYERS - Neural network building blocks
 * ============================================================================ */

typedef struct {
    VariableV2 *weight;  /* [out_features, in_features] */
    VariableV2 *bias;    /* [out_features] */
} Linear;

typedef struct {
    VariableV2 *embeddings;  /* [vocab_size, embed_dim] */
    int vocab_size;
    int embed_dim;
} Embedding;

typedef struct {
    VariableV2 *gamma;  /* [dim] */
    VariableV2 *beta;   /* [dim] */
    double eps;
} LayerNorm;

/* Layer creation (parameters are persistent) */
Linear* linear_create(int in_features, int out_features);
Embedding* embedding_create(int vocab_size, int embed_dim);
LayerNorm* layernorm_create(int dim);

/* Layer forward (returns temp Variables) */
VariableV2* linear_forward(Linear *layer, VariableV2 *input);
VariableV2* embedding_forward(Embedding *layer, const int *indices, int seq_len);
VariableV2* layernorm_forward(LayerNorm *layer, VariableV2 *input);

/* ============================================================================
 * OPTIMIZER V2 - Simplified SGD optimizer
 * ============================================================================ */

struct OptimizerV2 {
    VariableV2 **parameters;
    int num_params;
    double lr;
};

OptimizerV2* optimizer_create(VariableV2 **params, int num_params, double lr);
void optimizer_zero_grad(OptimizerV2 *opt);
void optimizer_step(OptimizerV2 *opt);
void optimizer_destroy(OptimizerV2 *opt);

/* ============================================================================
 * GLOBAL STATE
 * ============================================================================ */

extern TapeV2 *g_tape;      /* Global tape for convenience */

/* Initialize/cleanup */
void autograd_v2_init(void);
void autograd_v2_cleanup(void);

/* Reset arena after each iteration */
void autograd_reset_iteration(void);

/* Additional operations for transformer */
VariableV2* var_reshape(VariableV2 *x, int *new_shape, int new_rank);
VariableV2* var_relu(VariableV2 *x);
VariableV2* var_softmax_2d(VariableV2 *x);
void tape_record_layer_norm(VariableV2 *output, VariableV2 *input,
                           VariableV2 *gamma, VariableV2 *beta,
                           double mean, double var);
void tape_record_layer_norm_v2(VariableV2 *output, VariableV2 *input,
                              VariableV2 *gamma, VariableV2 *beta,
                              double *means, double *vars, int n_positions);
void tape_record_cross_entropy(VariableV2 *loss, VariableV2 *logits,
                               int *targets, int seq_len);

/* Missing operations */
VariableV2* ag_multiply(VariableV2 *a, VariableV2 *b);
VariableV2* ag_transpose(VariableV2 *x);
#define var_add ag_add  /* Alias for consistency with transformer code */

/* Layer management */
void linear_free(Linear *layer);
void var_free_persistent(VariableV2 *v);

#endif /* AUTOGRAD_V2_H */