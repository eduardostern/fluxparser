# Batching Analysis: Why Full Batching Doesn't Work for CPU Transformers

**Date**: November 14, 2025

## Problem Statement

User reported training was "too slow" at ~13 it/s and producing gibberish after 5000 iterations.

Initial hypothesis: Implement PyTorch-style batched processing where all sequences are processed simultaneously.

## What I Tried: Full Batched Attention

Implemented batched transformer where:
- Input shape: `[(batch_size * seq_len), d_model]` = `[1024, 128]` (16 batches × 64 seq_len)
- Attention computed over all 1024 tokens simultaneously
- Single forward pass for entire batch

**Expected**: Faster processing via batched matrix operations
**Result**: **Complete failure** - program hung/timed out

## Root Cause: O(n²) Attention Complexity

Attention computation scales quadratically with sequence length:

| Approach | Attention Matrix Size | Operations per Head | Total (4 heads) |
|----------|----------------------|---------------------|-----------------|
| Sequential | 16 × (64 × 64) | 16 × 4,096 = 65,536 | 262,144 |
| Batched | 1 × (1024 × 1024) | 1,048,576 | 4,194,304 |

**Batched version is 16x SLOWER!**

## Why PyTorch Batching Works (But Not Here)

**PyTorch/GPU batching**:
- Parallel hardware - processes 1024×1024 matrix in parallel
- Memory bandwidth >> compute time
- Batching amortizes kernel launch overhead
- Result: ~10-100x speedup from batching

**CPU with BLAS batching**:
- Sequential processing - O(n²) work is O(n²) time
- Larger matrices = more work, not faster work
- No parallelism to exploit the batch dimension in attention
- Result: Linear slowdown proportional to batch size

## What Actually Works: Sequential Batch Processing

```c
/* Process sequences one by one, accumulate gradients */
VariableV2 *batch_loss = NULL;
for (int b = 0; b < config.batch_size; b++) {
    int *seq_inputs = batch_inputs + b * config.seq_len;
    int *seq_targets = batch_targets + b * config.seq_len;

    /* Each sequence: 64×64 attention (manageable) */
    VariableV2 *logits = transformer_forward(model, seq_inputs, config.seq_len);
    VariableV2 *loss = compute_cross_entropy_loss(logits, seq_targets, config.seq_len);

    /* Accumulate for gradient averaging */
    batch_loss = (b == 0) ? loss : var_add(batch_loss, loss);
}

/* Single backward pass on accumulated loss */
double loss_val = batch_loss->data->data[0] / config.batch_size;
batch_loss->grad->data[0] = 1.0;
tape_backward(g_tape);
```

**Performance**: 10-11 it/s, 535 MB memory ✅

## The Real Problem: Learning Rate Schedule

Analysis of old training run shows the actual issue:

```
Iter   500/3000 | Loss: 3.5496 | LR: 9.54e-05 | Speed: 10.4 it/s
Iter  1000/3000 | Loss: 3.5182 | LR: 7.81e-05 | Speed: 10.4 it/s
Iter  2000/3000 | Loss: 3.2852 | LR: 2.66e-05 | Speed: 10.5 it/s
Iter  3000/3000 | Loss: 3.2076 | LR: 2.93e-11 | Speed: 10.5 it/s  ← LR collapsed!
```

**Observations**:
1. Speed was fine (10.5 it/s)
2. Loss was improving (3.5 → 3.2)
3. Learning stopped because LR decayed to near-zero
4. Output showed some structure: "t", "m", "ha", "th", "hat" (real words forming!)

## Recommended Solutions

### 1. Fix Learning Rate Schedule
```c
/* Current: Too aggressive decay */
double decay = (double)(n_iters - iter) / n_iters;  // Linear to zero
lr = base_lr * decay;  // Goes to 0 at n_iters

/* Better: Cosine annealing with minimum LR */
double min_lr = base_lr * 0.1;  // Don't go below 10% of base
double progress = (double)iter / n_iters;
lr = min_lr + (base_lr - min_lr) * 0.5 * (1.0 + cos(M_PI * progress));
```

### 2. Train for Much Longer
- Character-level models need 10K-50K iterations
- Current training stops at 1K-3K iterations
- Recommendation: `./train_full --small 20000`

### 3. Reduce Batch Size for Faster Iterations
```c
.batch_size = 8,  // Instead of 16
```
- Faster iterations (less work per iteration)
- More frequent gradient updates
- Tradeoff: Higher gradient variance

### 4. Multi-threading (Future Work)
Could parallelize the sequential loop:
```c
#pragma omp parallel for
for (int b = 0; b < config.batch_size; b++) {
    // Process sequence b in separate thread
}
```
Potential speedup: 2-4x on multi-core CPUs

## Lessons Learned

1. **Batching is not universally faster**: Depends on computation type and hardware
2. **O(n²) complexity matters**: Quadratic operations don't scale well
3. **GPU optimizations != CPU optimizations**: Different bottlenecks
4. **Measure before optimizing**: The "slow" code was actually fine (10 it/s)
5. **Learning dynamics > compute speed**: Bad LR schedule > slow training

## Recommendations for User

**Don't change batch processing**. The sequential loop is optimal for CPU.

**Do this instead**:
1. Train for 20K+ iterations: `./train_full --small 20000`
2. Fix learning rate schedule (add cosine annealing)
3. Consider reducing batch_size to 8 for faster iterations
4. Be patient - character-level LMs need time to learn

**Expected results at 20K iterations**:
- Loss: ~2.5-2.0
- Output: Short Shakespeare-like words and phrases
- Training time: ~30 minutes at 10 it/s

---

**Conclusion**: The code was already optimized. The problem was training parameters, not implementation.
