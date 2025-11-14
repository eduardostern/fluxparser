/*
 * AUTOGRAD V2 - Implementation
 */

#include "autograd_v2.h"
#include "arena.h"
#include "blas_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

/* Global flag to enable/disable BLAS acceleration */
static int g_use_blas = 1;  /* Enabled by default if available */

/* Global state */
TapeV2 *g_tape = NULL;

/* ============================================================================
 * TENSOR V2 IMPLEMENTATION
 * ============================================================================ */

/* Helper: calculate tensor size from shape */
static int calculate_size(const int *shape, int rank) {
    int size = 1;
    for (int i = 0; i < rank; i++) {
        size *= shape[i];
    }
    return size;
}

/* Create persistent tensor (heap allocated) */
TensorV2* tensor_create_persistent(const int *shape, int rank) {
    TensorV2 *t = malloc(sizeof(TensorV2));
    if (!t) return NULL;

    t->rank = rank;
    t->size = calculate_size(shape, rank);
    t->storage = TENSOR_PERSISTENT;

    t->shape = malloc(rank * sizeof(int));
    memcpy(t->shape, shape, rank * sizeof(int));

    t->data = calloc(t->size, sizeof(double));
    if (!t->data) {
        free(t->shape);
        free(t);
        return NULL;
    }

    return t;
}

/* Create temporary tensor (arena allocated) */
TensorV2* tensor_create_temp(const int *shape, int rank) {
    if (!global_arena) {
        arena_init_global();
    }

    TensorV2 *t = arena_alloc(global_arena, sizeof(TensorV2));
    if (!t) return NULL;

    t->rank = rank;
    t->size = calculate_size(shape, rank);
    t->storage = TENSOR_TEMPORARY;

    t->shape = arena_alloc(global_arena, rank * sizeof(int));
    memcpy(t->shape, shape, rank * sizeof(int));

    t->data = arena_calloc(global_arena, t->size, sizeof(double));
    if (!t->data) return NULL;

    return t;
}

/* Create zero tensor (persistent) */
TensorV2* tensor_zeros_persistent(const int *shape, int rank) {
    return tensor_create_persistent(shape, rank);  /* Already zeros from calloc */
}

/* Create zero tensor (temp) */
TensorV2* tensor_zeros_temp(const int *shape, int rank) {
    return tensor_create_temp(shape, rank);  /* Already zeros from arena_calloc */
}

/* Create random normal tensor (persistent) */
TensorV2* tensor_randn_persistent(const int *shape, int rank, double scale) {
    TensorV2 *t = tensor_create_persistent(shape, rank);
    if (!t) return NULL;

    /* Box-Muller transform for normal distribution */
    for (int i = 0; i < t->size; i += 2) {
        double u1 = ((double)rand() + 1.0) / (RAND_MAX + 1.0);
        double u2 = (double)rand() / RAND_MAX;
        double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
        double z1 = sqrt(-2.0 * log(u1)) * sin(2.0 * M_PI * u2);

        t->data[i] = z0 * scale;
        if (i + 1 < t->size) {
            t->data[i + 1] = z1 * scale;
        }
    }

    return t;
}

/* Clone tensor to temporary */
TensorV2* tensor_clone_temp(const TensorV2 *src) {
    if (!src) return NULL;

    TensorV2 *t = tensor_create_temp(src->shape, src->rank);
    if (!t) return NULL;

    memcpy(t->data, src->data, src->size * sizeof(double));
    return t;
}

/* Free persistent tensor */
void tensor_free_persistent(TensorV2 *t) {
    if (!t || t->storage != TENSOR_PERSISTENT) return;

    free(t->data);
    free(t->shape);
    free(t);
}

/* Tensor addition */
TensorV2* tensor_add(const TensorV2 *a, const TensorV2 *b) {
    assert(a->size == b->size);

    TensorV2 *result = tensor_create_temp(a->shape, a->rank);
    for (int i = 0; i < a->size; i++) {
        result->data[i] = a->data[i] + b->data[i];
    }
    return result;
}

/* Tensor subtraction */
TensorV2* tensor_subtract(const TensorV2 *a, const TensorV2 *b) {
    assert(a->size == b->size);

    TensorV2 *result = tensor_create_temp(a->shape, a->rank);
    for (int i = 0; i < a->size; i++) {
        result->data[i] = a->data[i] - b->data[i];
    }
    return result;
}

/* Element-wise multiplication */
TensorV2* tensor_multiply(const TensorV2 *a, const TensorV2 *b) {
    assert(a->size == b->size);

    TensorV2 *result = tensor_create_temp(a->shape, a->rank);
    for (int i = 0; i < a->size; i++) {
        result->data[i] = a->data[i] * b->data[i];
    }
    return result;
}

/* Matrix multiplication */
TensorV2* tensor_matmul(const TensorV2 *a, const TensorV2 *b) {
    assert(a->rank == 2 && b->rank == 2);
    assert(a->shape[1] == b->shape[0]);

    int m = a->shape[0];
    int n = b->shape[1];
    int k = a->shape[1];

    int shape[] = {m, n};
    TensorV2 *result = tensor_create_temp(shape, 2);

    /* Use optimized BLAS if available, otherwise fallback to pure C */
    matmul_optimized(a->data, b->data, result->data, m, k, n, g_use_blas);

    return result;
}

/* Transpose (2D only) */
TensorV2* tensor_transpose(const TensorV2 *a) {
    assert(a->rank == 2);

    int shape[] = {a->shape[1], a->shape[0]};
    TensorV2 *result = tensor_create_temp(shape, 2);

    /* Use optimized transpose */
    transpose_optimized(a->data, result->data, a->shape[0], a->shape[1], g_use_blas);

    return result;
}

/* ReLU activation */
TensorV2* tensor_relu(const TensorV2 *x) {
    TensorV2 *result = tensor_create_temp(x->shape, x->rank);
    for (int i = 0; i < x->size; i++) {
        result->data[i] = x->data[i] > 0 ? x->data[i] : 0;
    }
    return result;
}

/* Softmax (assumes last dimension) */
TensorV2* tensor_softmax(const TensorV2 *x) {
    TensorV2 *result = tensor_create_temp(x->shape, x->rank);

    if (x->rank == 1) {
        /* 1D softmax */
        double max_val = x->data[0];
        for (int i = 1; i < x->size; i++) {
            if (x->data[i] > max_val) max_val = x->data[i];
        }

        double sum = 0.0;
        for (int i = 0; i < x->size; i++) {
            result->data[i] = exp(x->data[i] - max_val);
            sum += result->data[i];
        }

        for (int i = 0; i < x->size; i++) {
            result->data[i] /= sum;
        }
    } else if (x->rank == 2) {
        /* 2D softmax over last dimension */
        int batch = x->shape[0];
        int dim = x->shape[1];

        for (int b = 0; b < batch; b++) {
            int offset = b * dim;

            double max_val = x->data[offset];
            for (int i = 1; i < dim; i++) {
                if (x->data[offset + i] > max_val) {
                    max_val = x->data[offset + i];
                }
            }

            double sum = 0.0;
            for (int i = 0; i < dim; i++) {
                result->data[offset + i] = exp(x->data[offset + i] - max_val);
                sum += result->data[offset + i];
            }

            for (int i = 0; i < dim; i++) {
                result->data[offset + i] /= sum;
            }
        }
    }

    return result;
}

/* Sum all elements */
double tensor_sum(const TensorV2 *x) {
    double sum = 0.0;
    for (int i = 0; i < x->size; i++) {
        sum += x->data[i];
    }
    return sum;
}

/* ============================================================================
 * VARIABLE V2 IMPLEMENTATION
 * ============================================================================ */

/* Create parameter Variable (persistent) */
VariableV2* var_create_parameter(TensorV2 *data) {
    VariableV2 *var = malloc(sizeof(VariableV2));
    var->data = data;
    var->grad = tensor_zeros_persistent(data->shape, data->rank);
    var->requires_grad = true;
    var->is_parameter = true;
    return var;
}

/* Create temporary Variable (arena allocated) */
VariableV2* var_create_temp(TensorV2 *data, bool requires_grad) {
    if (!global_arena) {
        arena_init_global();
    }

    VariableV2 *var = arena_alloc(global_arena, sizeof(VariableV2));
    var->data = data;

    if (requires_grad) {
        var->grad = tensor_zeros_temp(data->shape, data->rank);
    } else {
        var->grad = NULL;
    }

    var->requires_grad = requires_grad;
    var->is_parameter = false;
    return var;
}

/* Zero gradients */
void var_zero_grad(VariableV2 *var) {
    if (!var || !var->grad) return;

    for (int i = 0; i < var->grad->size; i++) {
        var->grad->data[i] = 0.0;
    }
}

/* ============================================================================
 * TAPE V2 IMPLEMENTATION
 * ============================================================================ */

/* Create tape */
TapeV2* tape_create(void) {
    TapeV2 *tape = malloc(sizeof(TapeV2));
    tape->capacity = 1000;
    tape->count = 0;
    tape->ops = malloc(tape->capacity * sizeof(TapeOp));
    return tape;
}

/* Destroy tape */
void tape_destroy(TapeV2 *tape) {
    if (!tape) return;

    /* Free operation contexts */
    for (int i = 0; i < tape->count; i++) {
        if (tape->ops[i].ctx) {
            free(tape->ops[i].ctx);
        }
    }

    free(tape->ops);
    free(tape);
}

/* Reset tape (clear operations) */
void tape_reset(TapeV2 *tape) {
    if (!tape) return;

    /* Don't free contexts - they're arena allocated and will be
     * freed when arena_reset is called in autograd_reset_iteration */
    tape->count = 0;

    /* Shrink tape if it grew too large (prevents memory accumulation) */
    #define TAPE_MAX_CAPACITY 10000
    if (tape->capacity > TAPE_MAX_CAPACITY) {
        tape->capacity = TAPE_MAX_CAPACITY;
        tape->ops = realloc(tape->ops, tape->capacity * sizeof(TapeOp));
    }
}

/* Add operation to tape */
void tape_add_op(TapeV2 *tape, VariableV2 **inputs, int num_inputs,
                 VariableV2 *output, BackwardFunc backward, void *ctx) {
    if (!tape || !output->requires_grad) return;

    /* Expand if needed */
    if (tape->count >= tape->capacity) {
        tape->capacity *= 2;
        tape->ops = realloc(tape->ops, tape->capacity * sizeof(TapeOp));
    }

    TapeOp *op = &tape->ops[tape->count++];
    op->inputs = inputs;
    op->num_inputs = num_inputs;
    op->output = output;
    op->backward = backward;
    op->ctx = ctx;
}

/* Backward pass */
void tape_backward(TapeV2 *tape) {
    if (!tape) return;

    /* Process operations in reverse order */
    for (int i = tape->count - 1; i >= 0; i--) {
        TapeOp *op = &tape->ops[i];
        if (op->backward && op->output->grad) {
            op->backward(op->ctx, op->output->grad);
        }
    }
}

/* ============================================================================
 * AUTOGRAD OPERATIONS
 * ============================================================================ */

/* Context for add backward */
typedef struct {
    VariableV2 *a;
    VariableV2 *b;
} AddCtx;

static void backward_add(void *ctx, TensorV2 *grad_output) {
    AddCtx *c = (AddCtx*)ctx;

    if (c->a->requires_grad && c->a->grad) {
        /* Gradient for a is just grad_output */
        for (int i = 0; i < c->a->grad->size; i++) {
            c->a->grad->data[i] += grad_output->data[i];
        }
    }

    if (c->b->requires_grad && c->b->grad) {
        /* Check if b was broadcast */
        if (grad_output->rank == 2 && c->b->data->rank == 1) {
            /* Sum gradients across batch dimension */
            int batch = grad_output->shape[0];
            int features = grad_output->shape[1];

            for (int j = 0; j < features; j++) {
                double sum = 0.0;
                for (int i = 0; i < batch; i++) {
                    sum += grad_output->data[i * features + j];
                }
                c->b->grad->data[j] += sum;
            }
        } else {
            /* Regular case - same shape */
            for (int i = 0; i < c->b->grad->size; i++) {
                c->b->grad->data[i] += grad_output->data[i];
            }
        }
    }

    /* Don't free - context is arena allocated */
}

VariableV2* ag_add(VariableV2 *a, VariableV2 *b) {
    /* Handle broadcasting for bias addition */
    TensorV2 *result;

    if (a->data->rank == 2 && b->data->rank == 1) {
        /* a is [batch, features], b is [features] - broadcast b */
        int batch = a->data->shape[0];
        int features = a->data->shape[1];
        assert(b->data->size == features);

        result = tensor_create_temp(a->data->shape, a->data->rank);
        for (int i = 0; i < batch; i++) {
            for (int j = 0; j < features; j++) {
                result->data[i * features + j] =
                    a->data->data[i * features + j] + b->data->data[j];
            }
        }
    } else {
        /* Regular element-wise addition */
        result = tensor_add(a->data, b->data);
    }

    bool requires_grad = a->requires_grad || b->requires_grad;
    VariableV2 *output = var_create_temp(result, requires_grad);

    if (g_tape && requires_grad) {
        AddCtx *ctx = arena_alloc(global_arena, sizeof(AddCtx));
        ctx->a = a;
        ctx->b = b;

        VariableV2 *inputs[] = {a, b};
        tape_add_op(g_tape, inputs, 2, output, backward_add, ctx);
    }

    return output;
}

/* Context for matmul backward */
typedef struct {
    TensorV2 *a_data;
    TensorV2 *b_data;
    VariableV2 *a;
    VariableV2 *b;
} MatmulCtx;

static void backward_matmul(void *ctx, TensorV2 *grad_output) {
    MatmulCtx *c = (MatmulCtx*)ctx;

    if (c->a->requires_grad && c->a->grad) {
        /* grad_a = grad_output @ b^T */
        TensorV2 *b_T = tensor_transpose(c->b_data);
        TensorV2 *grad_a = tensor_matmul(grad_output, b_T);

        for (int i = 0; i < c->a->grad->size; i++) {
            c->a->grad->data[i] += grad_a->data[i];
        }
    }

    if (c->b->requires_grad && c->b->grad) {
        /* grad_b = a^T @ grad_output */
        TensorV2 *a_T = tensor_transpose(c->a_data);
        TensorV2 *grad_b = tensor_matmul(a_T, grad_output);

        for (int i = 0; i < c->b->grad->size; i++) {
            c->b->grad->data[i] += grad_b->data[i];
        }
    }

    /* Don't free - context is arena allocated */
}

VariableV2* ag_matmul(VariableV2 *a, VariableV2 *b) {
    TensorV2 *result = tensor_matmul(a->data, b->data);
    bool requires_grad = a->requires_grad || b->requires_grad;
    VariableV2 *output = var_create_temp(result, requires_grad);

    if (g_tape && requires_grad) {
        MatmulCtx *ctx = arena_alloc(global_arena, sizeof(MatmulCtx));
        ctx->a_data = tensor_clone_temp(a->data);
        ctx->b_data = tensor_clone_temp(b->data);
        ctx->a = a;
        ctx->b = b;

        VariableV2 *inputs[] = {a, b};
        tape_add_op(g_tape, inputs, 2, output, backward_matmul, ctx);
    }

    return output;
}

/* Context for ReLU backward */
typedef struct {
    TensorV2 *input_data;
    VariableV2 *input;
} ReluCtx;

static void backward_relu(void *ctx, TensorV2 *grad_output) {
    ReluCtx *c = (ReluCtx*)ctx;

    if (c->input->requires_grad && c->input->grad) {
        for (int i = 0; i < grad_output->size; i++) {
            if (c->input_data->data[i] > 0) {
                c->input->grad->data[i] += grad_output->data[i];
            }
        }
    }

    /* Don't free - context is arena allocated */
}

VariableV2* ag_relu(VariableV2 *x) {
    TensorV2 *result = tensor_relu(x->data);
    VariableV2 *output = var_create_temp(result, x->requires_grad);

    if (g_tape && x->requires_grad) {
        ReluCtx *ctx = arena_alloc(global_arena, sizeof(ReluCtx));
        ctx->input_data = tensor_clone_temp(x->data);
        ctx->input = x;

        VariableV2 *inputs[] = {x};
        tape_add_op(g_tape, inputs, 1, output, backward_relu, ctx);
    }

    return output;
}

/* Placeholder for ag_cross_entropy_loss - not yet implemented */
VariableV2* ag_cross_entropy_loss(VariableV2 *logits, VariableV2 *targets) {
    (void)logits;   /* Suppress unused parameter warning */
    (void)targets;  /* Suppress unused parameter warning */

    /* For now, just return a dummy loss */
    int shape[] = {1};
    TensorV2 *loss_tensor = tensor_create_temp(shape, 1);
    loss_tensor->data[0] = 0.0;
    return var_create_temp(loss_tensor, false);
}

/* ============================================================================
 * LAYERS
 * ============================================================================ */

/* Create linear layer */
Linear* linear_create(int in_features, int out_features) {
    Linear *layer = malloc(sizeof(Linear));

    /* Xavier initialization */
    double scale = sqrt(2.0 / in_features);

    int w_shape[] = {out_features, in_features};
    TensorV2 *w = tensor_randn_persistent(w_shape, 2, scale);
    layer->weight = var_create_parameter(w);

    int b_shape[] = {out_features};
    TensorV2 *b = tensor_zeros_persistent(b_shape, 1);
    layer->bias = var_create_parameter(b);

    return layer;
}

/* Linear forward */
VariableV2* linear_forward(Linear *layer, VariableV2 *input) {
    /* output = input @ weight^T + bias */
    VariableV2 *wT = ag_transpose(layer->weight);
    VariableV2 *wx = ag_matmul(input, wT);

    /* For now, just use the bias directly - ag_add will handle broadcasting */
    /* TODO: Implement proper broadcast with gradient flow back to original bias */

    return ag_add(wx, layer->bias);
}

/* Create embedding layer */
Embedding* embedding_create(int vocab_size, int embed_dim) {
    Embedding *layer = malloc(sizeof(Embedding));
    layer->vocab_size = vocab_size;
    layer->embed_dim = embed_dim;

    int shape[] = {vocab_size, embed_dim};
    double scale = 0.01;
    TensorV2 *emb = tensor_randn_persistent(shape, 2, scale);
    layer->embeddings = var_create_parameter(emb);

    return layer;
}

/* Embedding forward */
VariableV2* embedding_forward(Embedding *layer, const int *indices, int seq_len) {
    int shape[] = {seq_len, layer->embed_dim};
    TensorV2 *output = tensor_create_temp(shape, 2);

    /* Lookup embeddings */
    for (int i = 0; i < seq_len; i++) {
        int idx = indices[i];
        for (int j = 0; j < layer->embed_dim; j++) {
            output->data[i * layer->embed_dim + j] =
                layer->embeddings->data->data[idx * layer->embed_dim + j];
        }
    }

    /* TODO: Add backward pass for embedding */
    return var_create_temp(output, false);  /* No grad for now */
}

/* Create layer norm */
LayerNorm* layernorm_create(int dim) {
    LayerNorm *layer = malloc(sizeof(LayerNorm));
    layer->eps = 1e-5;

    int shape[] = {dim};
    TensorV2 *gamma = tensor_create_persistent(shape, 1);
    TensorV2 *beta = tensor_zeros_persistent(shape, 1);

    /* Initialize gamma to 1 */
    for (int i = 0; i < dim; i++) {
        gamma->data[i] = 1.0;
    }

    layer->gamma = var_create_parameter(gamma);
    layer->beta = var_create_parameter(beta);

    return layer;
}

/* Layer norm forward (simplified) */
VariableV2* layernorm_forward(LayerNorm *layer, VariableV2 *input) {
    /* Simplified: just return scaled input for now */
    /* TODO: Implement proper layer norm with mean/variance */
    return ag_multiply(input, layer->gamma);
}

/* ============================================================================
 * OPTIMIZER
 * ============================================================================ */

/* Create optimizer */
OptimizerV2* optimizer_create(VariableV2 **params, int num_params, double lr) {
    OptimizerV2 *opt = malloc(sizeof(OptimizerV2));
    opt->parameters = params;
    opt->num_params = num_params;
    opt->lr = lr;
    return opt;
}

/* Zero gradients */
void optimizer_zero_grad(OptimizerV2 *opt) {
    for (int i = 0; i < opt->num_params; i++) {
        var_zero_grad(opt->parameters[i]);
    }
}

/* Update parameters */
void optimizer_step(OptimizerV2 *opt) {
    for (int i = 0; i < opt->num_params; i++) {
        VariableV2 *param = opt->parameters[i];
        if (param->grad) {
            for (int j = 0; j < param->data->size; j++) {
                param->data->data[j] -= opt->lr * param->grad->data[j];
            }
        }
    }
}

/* Destroy optimizer */
void optimizer_destroy(OptimizerV2 *opt) {
    free(opt);
}

/* ============================================================================
 * GLOBAL STATE MANAGEMENT
 * ============================================================================ */

/* Initialize autograd */
void autograd_v2_init(void) {
    arena_init_global();
    g_tape = tape_create();

    /* Print BLAS acceleration info */
    if (has_blas()) {
        printf("🚀 BLAS acceleration: %s\n", get_blas_impl());
    } else {
        printf("⚠️  Using pure C (no BLAS) - training will be slower\n");
    }
}

/* Cleanup */
void autograd_v2_cleanup(void) {
    tape_destroy(g_tape);
    g_tape = NULL;
    arena_cleanup_global();
}

/* Reset iteration (frees all temporaries) */
void autograd_reset_iteration(void) {
    static int iteration_count = 0;
    iteration_count++;

    tape_reset(g_tape);
    if (global_arena) {
        /* Every 10 iterations, aggressively free memory to prevent unbounded growth */
        if (iteration_count % 10 == 0) {
            arena_reset_aggressive(global_arena);
        } else {
            arena_reset(global_arena);
        }
    }
}

/* Context for transpose backward */
typedef struct {
    VariableV2 *input;
} TransposeCtx;

static void backward_transpose(void *ctx, TensorV2 *grad_output) {
    TransposeCtx *c = (TransposeCtx*)ctx;

    if (c->input->requires_grad && c->input->grad) {
        /* Gradient of transpose is just transpose of the gradient */
        TensorV2 *grad_transposed = tensor_transpose(grad_output);

        for (int i = 0; i < c->input->grad->size; i++) {
            c->input->grad->data[i] += grad_transposed->data[i];
        }
    }

    /* Don't free - context is arena allocated */
}

/* Transpose operation */
VariableV2* ag_transpose(VariableV2 *x) {
    TensorV2 *result = tensor_transpose(x->data);
    VariableV2 *output = var_create_temp(result, x->requires_grad);

    if (g_tape && x->requires_grad) {
        TransposeCtx *ctx = arena_alloc(global_arena, sizeof(TransposeCtx));
        ctx->input = x;

        VariableV2 *inputs[] = {x};
        tape_add_op(g_tape, inputs, 1, output, backward_transpose, ctx);
    }

    return output;
}

/* ============================================================================
 * ADDITIONAL OPERATIONS FOR TRANSFORMER
 * ============================================================================ */

/* Reshape variable */
/* Context for reshape backward */
typedef struct {
    VariableV2 *input;
    int *original_shape;
    int original_rank;
} ReshapeCtx;

static void backward_reshape(void *ctx, TensorV2 *grad_output) {
    ReshapeCtx *c = (ReshapeCtx*)ctx;

    /* Reshape gradient back to original shape */
    if (c->input->requires_grad && c->input->grad) {
        /* Just copy gradients since reshape is just a view */
        for (int i = 0; i < grad_output->size; i++) {
            c->input->grad->data[i] += grad_output->data[i];
        }
    }

    /* Don't free - arena allocated */
}

VariableV2* var_reshape(VariableV2 *x, int *new_shape, int new_rank) {
    /* Verify total size matches */
    int new_size = 1;
    for (int i = 0; i < new_rank; i++) {
        new_size *= new_shape[i];
    }
    assert(new_size == x->data->size);

    /* Create reshaped tensor (view of same data) */
    TensorV2 *reshaped = tensor_create_temp(new_shape, new_rank);
    memcpy(reshaped->data, x->data->data, new_size * sizeof(double));

    VariableV2 *output = var_create_temp(reshaped, x->requires_grad);

    /* Record for backward pass */
    if (x->requires_grad && g_tape) {
        ReshapeCtx *ctx = arena_alloc(global_arena, sizeof(ReshapeCtx));
        ctx->input = x;
        ctx->original_rank = x->data->rank;
        /* Allocate shape in arena to avoid leak */
        ctx->original_shape = arena_alloc(global_arena, x->data->rank * sizeof(int));
        memcpy(ctx->original_shape, x->data->shape, x->data->rank * sizeof(int));

        VariableV2 *inputs[] = {x};
        tape_add_op(g_tape, inputs, 1, output, backward_reshape, ctx);
    }

    return output;
}

/* ReLU activation */
VariableV2* var_relu(VariableV2 *x) {
    TensorV2 *result = tensor_create_temp(x->data->shape, x->data->rank);

    for (int i = 0; i < x->data->size; i++) {
        result->data[i] = fmax(0.0, x->data->data[i]);
    }

    VariableV2 *output = var_create_temp(result, x->requires_grad);

    /* Record for backward pass */
    if (x->requires_grad && g_tape) {
        ReluCtx *ctx = arena_alloc(global_arena, sizeof(ReluCtx));
        ctx->input_data = tensor_clone_temp(x->data);
        ctx->input = x;

        VariableV2 *inputs[] = {x};
        tape_add_op(g_tape, inputs, 1, output, backward_relu, ctx);
    }

    return output;
}

/* Context for softmax backward */
typedef struct {
    VariableV2 *input;
    TensorV2 *output_data;  /* Store softmax output for backward */
} SoftmaxCtx;

static void backward_softmax_2d(void *ctx, TensorV2 *grad_output) {
    SoftmaxCtx *c = (SoftmaxCtx*)ctx;

    if (c->input->requires_grad && c->input->grad) {
        int batch_size = 1;
        for (int i = 0; i < grad_output->rank - 1; i++) {
            batch_size *= grad_output->shape[i];
        }
        int dim = grad_output->shape[grad_output->rank - 1];

        for (int b = 0; b < batch_size; b++) {
            int offset = b * dim;

            /* Compute Jacobian-vector product */
            for (int i = 0; i < dim; i++) {
                double sum = 0.0;
                for (int j = 0; j < dim; j++) {
                    double jacobian_ij;
                    if (i == j) {
                        jacobian_ij = c->output_data->data[offset + i] *
                                      (1.0 - c->output_data->data[offset + i]);
                    } else {
                        jacobian_ij = -c->output_data->data[offset + i] *
                                      c->output_data->data[offset + j];
                    }
                    sum += jacobian_ij * grad_output->data[offset + j];
                }
                c->input->grad->data[offset + i] += sum;
            }
        }
    }
}

/* Softmax over 2D tensor (last dimension) */
VariableV2* var_softmax_2d(VariableV2 *x) {
    assert(x->data->rank >= 2);

    int batch_size = 1;
    for (int i = 0; i < x->data->rank - 1; i++) {
        batch_size *= x->data->shape[i];
    }
    int dim = x->data->shape[x->data->rank - 1];

    TensorV2 *result = tensor_create_temp(x->data->shape, x->data->rank);

    for (int b = 0; b < batch_size; b++) {
        /* Find max for numerical stability */
        double max_val = -INFINITY;
        for (int i = 0; i < dim; i++) {
            double val = x->data->data[b * dim + i];
            if (val > max_val) max_val = val;
        }

        /* Compute exp and sum */
        double sum = 0.0;
        for (int i = 0; i < dim; i++) {
            result->data[b * dim + i] = exp(x->data->data[b * dim + i] - max_val);
            sum += result->data[b * dim + i];
        }

        /* Normalize */
        for (int i = 0; i < dim; i++) {
            result->data[b * dim + i] /= sum;
        }
    }

    VariableV2 *output = var_create_temp(result, x->requires_grad);

    /* Record for backward pass */
    if (x->requires_grad && g_tape) {
        SoftmaxCtx *ctx = arena_alloc(global_arena, sizeof(SoftmaxCtx));
        ctx->input = x;
        ctx->output_data = tensor_clone_temp(result);

        VariableV2 *inputs[] = {x};
        tape_add_op(g_tape, inputs, 1, output, backward_softmax_2d, ctx);
    }

    return output;
}

/* Context for layer norm backward */
typedef struct {
    VariableV2 *input;
    VariableV2 *gamma;
    VariableV2 *beta;
    double *means;    /* Mean for each position */
    double *vars;     /* Variance for each position */
    int n_positions;  /* Number of positions normalized */
    int d_model;      /* Size of each position */
} LayerNormCtx;

static void backward_layer_norm(void *ctx, TensorV2 *grad_output) {
    LayerNormCtx *c = (LayerNormCtx*)ctx;
    int d_model = c->d_model;
    int n_positions = c->n_positions;
    double eps = 1e-5;

    /* Accumulate gradients for gamma and beta across all positions */
    if (c->gamma->requires_grad && c->gamma->grad) {
        for (int pos = 0; pos < n_positions; pos++) {
            double mean = c->means[pos];
            double var = c->vars[pos];
            double std = sqrt(var + eps);

            int offset = pos * d_model;
            for (int i = 0; i < d_model; i++) {
                double x_normalized = (c->input->data->data[offset + i] - mean) / std;
                c->gamma->grad->data[i] += grad_output->data[offset + i] * x_normalized;
            }
        }
    }

    if (c->beta->requires_grad && c->beta->grad) {
        for (int pos = 0; pos < n_positions; pos++) {
            int offset = pos * d_model;
            for (int i = 0; i < d_model; i++) {
                c->beta->grad->data[i] += grad_output->data[offset + i];
            }
        }
    }

    /* Compute gradient for input */
    if (c->input->requires_grad && c->input->grad) {
        for (int pos = 0; pos < n_positions; pos++) {
            double mean = c->means[pos];
            double var = c->vars[pos];
            double std = sqrt(var + eps);
            int offset = pos * d_model;

            /* Compute intermediate values for this position */
            double grad_mean = 0.0;
            double grad_var = 0.0;

            /* First pass: compute grad_mean and grad_var */
            for (int i = 0; i < d_model; i++) {
                double grad_x_norm = grad_output->data[offset + i] * c->gamma->data->data[i];

                grad_var += grad_x_norm * (c->input->data->data[offset + i] - mean) * (-0.5) * pow(var + eps, -1.5);
                grad_mean += grad_x_norm * (-1.0 / std);
            }

            /* Add contribution from grad_var to grad_mean */
            for (int i = 0; i < d_model; i++) {
                grad_mean += grad_var * 2.0 * (c->input->data->data[offset + i] - mean) / d_model;
            }

            /* Second pass: compute final gradient */
            for (int i = 0; i < d_model; i++) {
                double grad_x_norm = grad_output->data[offset + i] * c->gamma->data->data[i];
                c->input->grad->data[offset + i] += grad_x_norm / std
                    + grad_var * 2.0 * (c->input->data->data[offset + i] - mean) / d_model
                    + grad_mean / d_model;
            }
        }
    }

    /* Don't free - arena allocated */
}

void tape_record_layer_norm(VariableV2 *output, VariableV2 *input,
                           VariableV2 *gamma, VariableV2 *beta,
                           double mean, double var) {
    if (input->requires_grad && g_tape) {
        /* Create context for backward pass */
        LayerNormCtx *ctx = (LayerNormCtx*)arena_alloc(global_arena, sizeof(LayerNormCtx));
        ctx->input = input;
        ctx->gamma = gamma;
        ctx->beta = beta;

        /* For now, store single mean/var - will need to fix for batch processing */
        ctx->n_positions = input->data->size / gamma->data->size;
        ctx->d_model = gamma->data->size;

        /* Allocate arrays for means and vars in arena */
        ctx->means = (double*)arena_alloc(global_arena, ctx->n_positions * sizeof(double));
        ctx->vars = (double*)arena_alloc(global_arena, ctx->n_positions * sizeof(double));

        /* For now, just store the last mean/var for all positions (simplified) */
        for (int i = 0; i < ctx->n_positions; i++) {
            ctx->means[i] = mean;
            ctx->vars[i] = var;
        }

        VariableV2 *inputs[] = {input, gamma, beta};
        tape_add_op(g_tape, inputs, 3, output, backward_layer_norm, ctx);
    }
}

void tape_record_layer_norm_v2(VariableV2 *output, VariableV2 *input,
                              VariableV2 *gamma, VariableV2 *beta,
                              double *means, double *vars, int n_positions) {
    if (input->requires_grad && g_tape) {
        /* Create context for backward pass */
        LayerNormCtx *ctx = (LayerNormCtx*)arena_alloc(global_arena, sizeof(LayerNormCtx));
        ctx->input = input;
        ctx->gamma = gamma;
        ctx->beta = beta;
        ctx->n_positions = n_positions;
        ctx->d_model = gamma->data->size;

        /* Copy means and vars in arena */
        ctx->means = (double*)arena_alloc(global_arena, n_positions * sizeof(double));
        ctx->vars = (double*)arena_alloc(global_arena, n_positions * sizeof(double));
        memcpy(ctx->means, means, n_positions * sizeof(double));
        memcpy(ctx->vars, vars, n_positions * sizeof(double));

        VariableV2 *inputs[] = {input, gamma, beta};
        tape_add_op(g_tape, inputs, 3, output, backward_layer_norm, ctx);
    }
}

/* Context for cross-entropy backward */
typedef struct {
    VariableV2 *logits;
    int *targets;
    int seq_len;
} CrossEntropyCtx;

static void backward_cross_entropy(void *ctx, TensorV2 *grad_output) {
    CrossEntropyCtx *c = (CrossEntropyCtx*)ctx;

    if (!c->logits->requires_grad || !c->logits->grad) return;

    int vocab_size = c->logits->data->shape[c->logits->data->rank - 1];

    /* For each position in sequence */
    for (int t = 0; t < c->seq_len; t++) {
        /* Compute softmax */
        double max_val = -INFINITY;
        for (int v = 0; v < vocab_size; v++) {
            double val = c->logits->data->data[t * vocab_size + v];
            if (val > max_val) max_val = val;
        }

        double sum = 0.0;
        for (int v = 0; v < vocab_size; v++) {
            double exp_val = exp(c->logits->data->data[t * vocab_size + v] - max_val);
            sum += exp_val;
        }

        /* Gradient is (softmax - one_hot) * grad_output */
        for (int v = 0; v < vocab_size; v++) {
            double softmax_val = exp(c->logits->data->data[t * vocab_size + v] - max_val) / sum;
            double grad = softmax_val;
            if (v == c->targets[t]) {
                grad -= 1.0;
            }
            /* Scale by output gradient and average over sequence */
            c->logits->grad->data[t * vocab_size + v] +=
                grad * grad_output->data[0] / c->seq_len;
        }
    }

    /* Don't free - context is arena allocated */
}

void tape_record_cross_entropy(VariableV2 *loss, VariableV2 *logits,
                               int *targets, int seq_len) {
    if (logits->requires_grad && g_tape) {
        CrossEntropyCtx *ctx = arena_alloc(global_arena, sizeof(CrossEntropyCtx));
        ctx->logits = logits;
        /* Copy targets to arena */
        ctx->targets = arena_alloc(global_arena, seq_len * sizeof(int));
        memcpy(ctx->targets, targets, seq_len * sizeof(int));
        ctx->seq_len = seq_len;

        VariableV2 *inputs[] = {logits};
        tape_add_op(g_tape, inputs, 1, loss, backward_cross_entropy, ctx);
    }
}

/* Layer management */
void linear_free(Linear *layer) {
    if (layer) {
        var_free_persistent(layer->weight);
        var_free_persistent(layer->bias);
        free(layer);
    }
}

void var_free_persistent(VariableV2 *v) {
    if (!v) return;

    /* Free persistent data if it exists */
    if (v->data && v->data->storage == TENSOR_PERSISTENT) {
        tensor_free_persistent(v->data);
    }

    /* Free persistent gradient if it exists */
    if (v->grad && v->grad->storage == TENSOR_PERSISTENT) {
        tensor_free_persistent(v->grad);
    }

    /* Free the Variable itself (only if it's a parameter) */
    if (v->is_parameter) {
        free(v);
    }
}

/* Context for multiply backward */
typedef struct {
    TensorV2 *a_data;
    TensorV2 *b_data;
    VariableV2 *a;
    VariableV2 *b;
} MultiplyCtx;

static void backward_multiply(void *ctx, TensorV2 *grad_output) {
    MultiplyCtx *c = (MultiplyCtx*)ctx;

    if (c->a->requires_grad && c->a->grad) {
        /* grad_a = grad_output * b */
        for (int i = 0; i < grad_output->size; i++) {
            c->a->grad->data[i] += grad_output->data[i] * c->b_data->data[i];
        }
    }

    if (c->b->requires_grad && c->b->grad) {
        /* grad_b = grad_output * a */
        for (int i = 0; i < grad_output->size; i++) {
            c->b->grad->data[i] += grad_output->data[i] * c->a_data->data[i];
        }
    }

    /* Don't free - context is arena allocated */
}

/* Multiply operation */
VariableV2* ag_multiply(VariableV2 *a, VariableV2 *b) {
    TensorV2 *result = tensor_multiply(a->data, b->data);
    bool requires_grad = a->requires_grad || b->requires_grad;
    VariableV2 *output = var_create_temp(result, requires_grad);

    if (g_tape && requires_grad) {
        MultiplyCtx *ctx = arena_alloc(global_arena, sizeof(MultiplyCtx));
        ctx->a_data = tensor_clone_temp(a->data);
        ctx->b_data = tensor_clone_temp(b->data);
        ctx->a = a;
        ctx->b = b;

        VariableV2 *inputs[] = {a, b};
        tape_add_op(g_tape, inputs, 2, output, backward_multiply, ctx);
    }

    return output;
}