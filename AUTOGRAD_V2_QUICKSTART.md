# Autograd V2 Quick Start Guide

## TL;DR
Memory-safe automatic differentiation for C with arena allocation. Solves 50GB memory leaks, enables stable transformer training.

## Installation

```bash
# Clone and build
git clone <repo>
cd parser

# Compile core library
gcc -c autograd_v2.c arena.c -O2
ar rcs libautograd_v2.a autograd_v2.o arena.o

# Compile transformer example
gcc -o train train_v2.c autograd_v2.c arena.c transformer_v2.c \
    sampling.c text_utils.c -lm -O2
```

## Basic Usage

### 1. Initialize System

```c
#include "autograd_v2.h"

int main() {
    // Initialize autograd system
    autograd_v2_init();

    // Your code here

    // Cleanup
    autograd_v2_cleanup();
    return 0;
}
```

### 2. Create Tensors and Variables

```c
// Create persistent tensor (for parameters)
int shape[] = {10, 20};
TensorV2 *weight = tensor_create_persistent(shape, 2);
tensor_init_xavier(weight);  // Random initialization

// Create variable that tracks gradients
VariableV2 *W = var_create_persistent(weight, true);

// Create temporary tensor (for activations)
TensorV2 *input_tensor = tensor_create_temp(shape, 2);
VariableV2 *x = var_create_temp(input_tensor, true);
```

### 3. Forward Pass Operations

```c
// Matrix multiplication
VariableV2 *y = ag_matmul(W, x);

// Addition with broadcasting
VariableV2 *z = ag_add(y, bias);

// ReLU activation
VariableV2 *h = var_relu(z);

// Reshape
int new_shape[] = {5, 40};
VariableV2 *reshaped = var_reshape(h, new_shape, 2);
```

### 4. Compute Loss and Backward Pass

```c
// Compute cross-entropy loss
VariableV2 *logits = model_forward(input);
VariableV2 *loss = compute_cross_entropy_loss(logits, targets, seq_len);

// Set gradient for loss (always 1.0)
loss->grad->data[0] = 1.0;

// Run backward pass
tape_backward(g_tape);

// Now all variables have gradients computed!
printf("Weight gradient sum: %.4f\n", tensor_sum(W->grad));
```

### 5. Update Weights (SGD Example)

```c
// Simple SGD update
void sgd_step(VariableV2 **params, int n_params, double lr) {
    for (int i = 0; i < n_params; i++) {
        VariableV2 *p = params[i];
        if (p->grad) {
            for (int j = 0; j < p->data->size; j++) {
                p->data->data[j] -= lr * p->grad->data[j];
            }
        }
    }
}
```

### 6. Reset for Next Iteration

```c
// CRITICAL: Reset arena after each iteration
autograd_reset_iteration();
```

## Complete Training Loop Example

```c
#include "autograd_v2.h"
#include "transformer_v2.h"

int main() {
    autograd_v2_init();

    // Create model
    TransformerV2 *model = transformer_create(
        vocab_size, d_model, n_heads, n_layers, d_ff, max_seq_len
    );

    // Get parameters for optimizer
    VariableV2 **params;
    int n_params;
    transformer_get_params(model, &params, &n_params);

    // Create optimizer
    AdamOptimizerV2 *opt = adam_create(0.001);
    for (int i = 0; i < n_params; i++) {
        adam_add_param(opt, params[i]);
    }

    // Training loop
    for (int iter = 0; iter < 1000; iter++) {
        // Forward pass
        VariableV2 *logits = transformer_forward(model, input_tokens, seq_len);

        // Compute loss
        VariableV2 *loss = compute_cross_entropy_loss(logits, target_tokens, seq_len);

        // Backward pass
        loss->grad->data[0] = 1.0;
        tape_backward(g_tape);

        // Update weights
        adam_step(opt);

        // Print progress
        if (iter % 100 == 0) {
            printf("Iter %d, Loss: %.4f\n", iter, loss->data->data[0]);
        }

        // CRITICAL: Reset arena
        autograd_reset_iteration();
    }

    // Cleanup
    adam_free(opt);
    free(params);
    transformer_free(model);
    autograd_v2_cleanup();

    return 0;
}
```

## Memory Management Rules

### ✅ DO

1. **Always call `autograd_reset_iteration()`** after each training iteration
2. **Use `tensor_create_persistent()`** for model parameters
3. **Use `tensor_create_temp()`** for activations
4. **Initialize autograd before use** with `autograd_v2_init()`
5. **Clean up when done** with `autograd_v2_cleanup()`

### ❌ DON'T

1. **Don't use `malloc()` for operation contexts** - use `arena_alloc()`
2. **Don't free temporary tensors manually** - arena handles it
3. **Don't forget to reset** - memory will grow unbounded
4. **Don't mix v1 and v2** - use only autograd_v2 functions

## Common Operations Reference

| Operation | Function | Example |
|-----------|----------|---------|
| Add | `ag_add(a, b)` | `y = ag_add(x, bias)` |
| Multiply | `ag_multiply(a, b)` | `y = ag_multiply(x, scale)` |
| MatMul | `ag_matmul(a, b)` | `y = ag_matmul(W, x)` |
| Transpose | `ag_transpose(x)` | `x_T = ag_transpose(x)` |
| ReLU | `var_relu(x)` | `h = var_relu(x)` |
| Softmax | `var_softmax_2d(x)` | `probs = var_softmax_2d(logits)` |
| Reshape | `var_reshape(x, shape, rank)` | `y = var_reshape(x, new_shape, 2)` |

## Debugging Tips

### Check Memory Usage

```c
// Print arena statistics
printf("Arena used: %zu bytes\n", arena_get_used(global_arena));
printf("Arena allocated: %zu bytes\n", arena_get_allocated(global_arena));
```

### Verify Gradients

```c
// After backward pass
void check_gradients(VariableV2 **params, int n_params) {
    for (int i = 0; i < n_params; i++) {
        if (params[i]->grad) {
            double grad_sum = 0;
            for (int j = 0; j < params[i]->grad->size; j++) {
                grad_sum += fabs(params[i]->grad->data[j]);
            }
            printf("Param %d gradient sum: %.6f\n", i, grad_sum);
        }
    }
}
```

### Memory Leak Detection

```bash
# Use valgrind
valgrind --leak-check=full ./your_program

# Monitor system memory during run
./your_program &
watch -n 1 "ps aux | grep your_program"
```

## Performance Tips

1. **Compile with optimization**: Use `-O2` or `-O3`
2. **Batch operations**: Process multiple examples together
3. **Reuse tensors**: Don't create new tensors unnecessarily
4. **Profile hotspots**: Use `gprof` or `perf` to find bottlenecks
5. **Consider BLAS**: For large matrices, BLAS can be much faster

## Troubleshooting

### Problem: Segmentation Fault
- Check tensor shapes match for operations
- Verify `autograd_v2_init()` was called
- Ensure not accessing freed memory after reset

### Problem: Memory Keeps Growing
- Ensure `autograd_reset_iteration()` is called
- Check for malloc() instead of arena_alloc()
- Verify not storing references to temporary tensors

### Problem: Gradients are Zero
- Check `requires_grad=true` on inputs
- Verify backward pass is called
- Ensure loss gradient is set to 1.0

### Problem: NaN in Gradients
- Check for division by zero
- Verify numerical stability in softmax
- Look for exploding gradients (reduce learning rate)

## Next Steps

1. Read `AUTOGRAD_V2_ARCHITECTURE.md` for deep technical details
2. Study `test_*.c` files for component examples
3. Look at `transformer_v2.c` for complex model example
4. Experiment with `train_v2.c` for full training pipeline

## Support

For issues or questions:
- Check existing test files for examples
- Read the architecture documentation
- Review the header files for API details
- Debug with GDB: `gdb ./your_program`

Happy training! 🚀