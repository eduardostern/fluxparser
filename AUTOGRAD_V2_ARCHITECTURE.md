# Autograd V2 Architecture Documentation

## Overview

Complete rewrite of the automatic differentiation system to solve catastrophic memory leaks (50GB+) in the original implementation. The new architecture uses arena allocation to efficiently manage memory for temporary values while keeping model parameters persistent.

## Problem Statement

The original autograd system had severe memory leaks:
- **Memory Usage**: 50GB+ for 2000 training iterations
- **Root Cause**: Intermediate Variables/Tensors created during forward pass were never freed
- **Failed Solutions**: Manual freeing caused double-free errors and segfaults

## Solution: Arena-Based Memory Management

### Core Design Principles

1. **Separation of Concerns**
   - **Persistent Memory**: Model parameters (weights, biases) - heap allocated, survive between iterations
   - **Temporary Memory**: Intermediate activations - arena allocated, bulk freed after each iteration

2. **Arena Allocator**
   - Fast bulk allocation from pre-allocated memory chunks
   - Zero fragmentation within iteration
   - O(1) reset operation to free all temporaries
   - Aggressive reset every 100 iterations to prevent unbounded growth

3. **Tape-Based Autodiff**
   - Operations recorded to tape during forward pass
   - Backward functions stored with contexts
   - Gradient accumulation during backward pass

## Architecture Components

### 1. Arena Allocator (`arena.h`, `arena.c`)

```c
typedef struct Arena {
    ArenaChunk *first;      // First memory chunk
    ArenaChunk *current;    // Current allocation chunk
    size_t chunk_size;      // Default chunk size (10MB)
    size_t total_allocated; // Total memory allocated
    size_t total_used;      // Currently used
} Arena;
```

**Key Functions:**
- `arena_alloc()` - Allocate from current chunk or grow
- `arena_reset()` - Mark all memory as free (keep chunks)
- `arena_reset_aggressive()` - Free all chunks except first (every 100 iterations)

### 2. Tensor System (`autograd_v2.h`)

```c
typedef enum {
    TENSOR_PERSISTENT,  // Heap allocated (for parameters)
    TENSOR_TEMPORARY    // Arena allocated (for activations)
} TensorStorage;

typedef struct TensorV2 {
    double *data;
    int *shape;
    int rank;
    int size;
    TensorStorage storage;
} TensorV2;
```

### 3. Variable System

```c
typedef struct VariableV2 {
    TensorV2 *data;      // Forward pass value
    TensorV2 *grad;      // Accumulated gradient
    bool requires_grad;   // Track gradients?
} VariableV2;
```

### 4. Computational Graph Tape

```c
typedef struct TapeOp {
    VariableV2 **inputs;           // Input variables
    int num_inputs;                // Number of inputs
    VariableV2 *output;            // Output variable
    backward_fn_t backward_fn;     // Gradient function
    void *ctx;                     // Operation context
} TapeOp;

typedef struct TapeV2 {
    TapeOp *ops;        // Recorded operations
    int count;          // Current operation count
    int capacity;       // Allocated capacity
} TapeV2;
```

## Backward Pass Implementations

### Layer Normalization
```c
static void backward_layer_norm(void *ctx, TensorV2 *grad_output) {
    LayerNormCtx *c = (LayerNormCtx*)ctx;

    // Compute gradients for gamma and beta (accumulated across positions)
    // Compute gradient for input using chain rule
    // Handle mean and variance gradients
}
```

**Key Insight**: Store means and variances for all positions during forward pass for correct gradient computation.

### Attention Mechanism
```c
// Multi-head attention backward pass
// Gradients flow through:
// 1. Output projection
// 2. Attention weights (softmax)
// 3. Query, Key, Value projections
```

### Reshape Operation
```c
static void backward_reshape(void *ctx, TensorV2 *grad_output) {
    // Reshape is just a view - copy gradients directly
    // No computation needed, just shape restoration
}
```

### Softmax
```c
static void backward_softmax_2d(void *ctx, TensorV2 *grad_output) {
    // Jacobian-vector product computation
    // jacobian[i,j] = softmax[i] * (delta[i,j] - softmax[j])
}
```

## Memory Management Strategy

### Iteration Lifecycle

1. **Forward Pass**
   - Allocate temporaries from arena
   - Record operations to tape
   - Store contexts in arena

2. **Backward Pass**
   - Traverse tape in reverse
   - Accumulate gradients
   - Contexts read but not freed

3. **Weight Update**
   - Apply optimizer to persistent parameters
   - Gradients used but not freed

4. **Reset Phase**
   ```c
   void autograd_reset_iteration(void) {
       static int iteration_count = 0;
       iteration_count++;

       tape_reset(g_tape);  // Clear tape

       if (iteration_count % 100 == 0) {
           arena_reset_aggressive(global_arena);  // Free chunks
       } else {
           arena_reset(global_arena);  // Just mark as unused
       }
   }
   ```

### Memory Layout Example

```
Iteration N:
+------------------+
| Arena Chunk 1    |
| - Activations    |
| - Contexts       |
| - Temp tensors   |
+------------------+
| Arena Chunk 2    | (allocated if needed)
| - More temps     |
+------------------+

After Reset (normal):
+------------------+
| Arena Chunk 1    | <- reused, marked empty
+------------------+
| Arena Chunk 2    | <- kept but unused
+------------------+

After Reset (aggressive, every 100 iters):
+------------------+
| Arena Chunk 1    | <- kept
+------------------+
  Chunk 2 freed!
```

## Performance Metrics

### Before Refactor
- **Memory Usage**: 50GB+ (unbounded growth)
- **Stability**: Crashes after ~100 iterations
- **Speed**: Slow due to memory pressure

### After Refactor
- **Memory Usage**: 6GB peak (properly bounded)
- **Stability**: 2000+ iterations without issues
- **Speed**: ~2-3x faster due to better cache locality
- **Model Size**: 291K parameters test transformer

## Critical Implementation Details

### 1. Context Allocation
All backward pass contexts MUST be arena-allocated:
```c
// CORRECT
LayerNormCtx *ctx = arena_alloc(global_arena, sizeof(LayerNormCtx));
ctx->means = arena_alloc(global_arena, n * sizeof(double));

// WRONG - will leak!
LayerNormCtx *ctx = malloc(sizeof(LayerNormCtx));
```

### 2. Tensor Creation
Use appropriate storage type:
```c
// For parameters
TensorV2 *weight = tensor_create_persistent(shape, rank);

// For activations
TensorV2 *activation = tensor_create_temp(shape, rank);
```

### 3. Gradient Initialization
Parameters need gradient tensors:
```c
VariableV2 *param = var_create_persistent(tensor, true);
// Automatically creates param->grad as persistent tensor
```

### 4. Operation Recording
Only record if gradients needed:
```c
if (input->requires_grad && g_tape) {
    // Create context and record operation
    tape_add_op(g_tape, inputs, n, output, backward_fn, ctx);
}
```

## File Structure

### Core Autograd
- `autograd_v2.h` - Public API
- `autograd_v2.c` - Implementation (~1100 lines)
- `arena.h` - Arena allocator interface
- `arena.c` - Arena implementation

### Transformer
- `transformer_v2.h` - Transformer using new autograd
- `transformer_v2.c` - Implementation with attention, layer norm, etc.

### Training
- `train_v2.c` - Main training loop
- `sampling.h/c` - Text generation utilities

### Testing
- `test_backward.c` - Basic gradient tests
- `test_layer_norm.c` - Layer normalization forward/backward
- `test_attention.c` - Attention mechanism tests
- `test_transformer_backward.c` - Full transformer gradients
- `test_minimal_crash.c` - Debugging utilities

## Usage Example

```c
// Initialize system
autograd_v2_init();

// Create model
TransformerV2 *model = transformer_create(
    vocab_size, d_model, n_heads, n_layers, d_ff, max_seq_len
);

// Training loop
for (int iter = 0; iter < n_iters; iter++) {
    // Forward pass
    VariableV2 *logits = transformer_forward(model, tokens, seq_len);

    // Compute loss
    VariableV2 *loss = compute_cross_entropy_loss(logits, targets, seq_len);

    // Backward pass
    loss->grad->data[0] = 1.0;
    tape_backward(g_tape);

    // Update weights
    adam_step(optimizer);

    // CRITICAL: Reset arena
    autograd_reset_iteration();
}

// Cleanup
autograd_v2_cleanup();
```

## Future Optimizations

### 1. Parallelization
- Thread-local arenas for batch parallelism
- Parallel attention heads
- SIMD for tensor operations

### 2. Memory Optimizations
- Custom allocator for small objects
- Tensor memory pooling
- Gradient checkpointing for very deep models

### 3. Performance
- Fused operations (e.g., add + relu)
- Better cache alignment
- BLAS integration for matrix operations

## Debugging Tips

### Memory Leaks
1. Check all contexts are arena-allocated
2. Verify `autograd_reset_iteration()` is called
3. Use valgrind: `valgrind --leak-check=full ./train_v2 100`

### Gradient Issues
1. Print gradient sums after backward pass
2. Check requires_grad propagation
3. Verify operation recording to tape

### Crashes
1. Check tensor shape compatibility
2. Verify arena has space (grow chunk size if needed)
3. Look for double-free (shouldn't happen with arena)

## Conclusion

The autograd v2 system successfully solves the memory crisis through careful architectural design. The separation of persistent and temporary memory, combined with arena allocation and aggressive reset strategies, provides a robust foundation for training neural networks in C without memory leaks.

**Key Achievement**: 50GB → 6GB memory usage with stable training for 2000+ iterations.