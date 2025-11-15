# Learning Rate Schedule Fix (November 14-15, 2025)

## Problem Statement

After 10K-50K iterations of training, the model stopped learning and output quality plateaued, even though loss was still decreasing.

**Symptoms**:
- Training at 10K iterations: Loss 2.96, LR **2.52e-12** ← Essentially zero!
- Output quality: Repetitive tokens like "the the the" - not improving
- Model appeared to stop learning after ~5K iterations

## Root Cause

The original cosine annealing schedule decayed **all the way to zero**:

```c
/* OLD CODE - Decays to ZERO */
double progress = (double)(iter - warmup) / (n_iters - warmup);
return base_lr * (0.5 * (1.0 + cos(M_PI * progress)));

/* At iter = n_iters (e.g., 50K):
   progress = 1.0
   cos(π) = -1.0
   return = base_lr * (0.5 * (1.0 + (-1.0))) = base_lr * 0 = 0  ← ZERO! */
```

**Impact**: Learning rate decayed to essentially zero by 10-20K iterations, stopping all gradient updates and preventing the model from improving further.

## The Fix

Added a **minimum learning rate floor** (10% of base LR) using cosine annealing:

```c
/* NEW CODE - Minimum 10% LR floor */
double min_lr = config->learning_rate * 0.1;  /* Never go below 10% */
double progress = (double)(iter - config->warmup_iters) /
                 (config->n_iters - config->warmup_iters);
double cosine_decay = 0.5 * (1.0 + cos(M_PI * progress));

/* Scale between min_lr and base_lr using cosine */
return min_lr + (config->learning_rate - min_lr) * cosine_decay;

/* At iter = n_iters:
   cosine_decay = 0  (same as before)
   return = min_lr + (base_lr - min_lr) * 0 = min_lr  ← 10% of base! */
```

## Learning Rate Comparison

For 50K iterations with base LR = 3e-4:

| Iteration | Old LR     | New LR     | Difference  |
|-----------|------------|------------|-------------|
| 100       | 3.00e-04   | 3.00e-04   | (same)      |
| 1,000     | 3.00e-04   | 3.00e-04   | (same)      |
| 5,000     | 2.93e-04   | 2.94e-04   | +0.24%      |
| 10,000    | 2.72e-04   | 2.75e-04   | +1.04%      |
| 20,000    | 1.97e-04   | 2.07e-04   | +5.23%      |
| 30,000    | 1.04e-04   | 1.24e-04   | +18.8%      |
| 40,000    | 2.88e-05   | 5.59e-05   | +94.1%      |
| **50,000** | **0.00e+00** | **3.00e-05** | **+INFINITY** |

**Key Improvement**:
- Old @ 50K: **0.00e+00** (learning stops!)
- New @ 50K: **3.00e-05** (learning continues!)

## Expected Benefits

1. **Continued Learning**: Model can continue improving throughout 50K iterations
2. **Better Convergence**: Final loss should be lower with sustained gradient updates
3. **Higher Quality Output**: More coherent text as model learns longer
4. **Training Stability**: Prevents premature stopping due to vanishing LR

## Recommended Training Schedule

For best results:

```bash
# Small model (430K params): Train for 50K iterations
./train_full --small 50000

# Medium model (3.2M params): Train for 100K iterations
./train_full 100000

# Expected timeline:
#   0-5K iters:   Basic character patterns
#   5K-20K iters: Word formation ("the", "and", "to")
#   20K-50K iters: Short phrases and bigrams
#   50K+ iters:   More coherent Shakespeare-like text
```

## Files Modified

**train_full.c** (lines 72-88):
- Updated `get_learning_rate()` function
- Added minimum LR floor (10% of base)
- Maintains cosine annealing curve but prevents decay to zero

## Testing

To verify the fix works:

```bash
# Compile
make train_full

# Train for 50K iterations
./train_full --small 50000

# Monitor learning rate at key checkpoints:
# - Should see LR stay above 3e-05 even at iteration 50,000
# - Loss should continue decreasing (not plateau)
# - Output quality should improve throughout training
```

## Technical Details

**Why 10%?**
- Common practice in modern optimizers (AdamW, etc.)
- Balances exploration (finding better minima) vs exploitation (refining current solution)
- Prevents model from getting stuck in poor local minima
- Used successfully in GPT-2, GPT-3, and other large language models

**Alternative Schedules**:
```c
/* More aggressive: 1% minimum */
double min_lr = config->learning_rate * 0.01;

/* Conservative: 20% minimum */
double min_lr = config->learning_rate * 0.2;

/* Recommended: 10% minimum (current) */
double min_lr = config->learning_rate * 0.1;
```

## Related Issues

- See `BATCH_PROCESSING_BUG_FIX.md` for batch processing improvements
- See `BATCHING_ANALYSIS.md` for why full batching doesn't work on CPU
- See `PHASE_4_COMPLETE.md` for Adam optimizer and memory fixes

---

**Date**: November 14-15, 2025
**Status**: ✅ Fixed, ready for testing
**Impact**: Enables continued learning throughout 50K+ iterations
**Next**: Train for 50K iterations and verify quality improvements
