# Current Limitation: Memory Leak on Long Training Runs

**Status**: Partially Fixed (works for short runs only)
**Date**: November 13, 2025

---

## Summary

The memory fixes we implemented work perfectly for **short training runs** (100 iterations) but there's still a memory leak that appears on longer runs (5000+ iterations).

## What Works ✅

**Short Training Runs:**
```bash
./train_full --small   # 100 iterations by default
# Result: 1.9GB memory, 100 it/s, stable ✅
```

## What Doesn't Work ❌

**Long Training Runs:**
```bash
./train_full --small 5000   # 5000+ iterations
# Result: Memory grows to 60GB, system becomes unusable ❌
```

##  Root Cause Analysis

###  What We Fixed (Phase 3)
1. ✅ **Arena reset timing** - Moved to end of loop
2. ✅ **BLAS acceleration** - 3000x speedup
3. ✅ **Aggressive arena cleanup** - Every 10 iterations

### What's Still Broken
There's memory accumulating somewhere that the arena reset isn't catching. Possibilities:

#### 1. Optimizer State Growth
```c
// AdamOptimizer allocates m and v tensors
typedef struct {
    double learning_rate;
    double beta1, beta2, epsilon;
    int t;  // timestep

    // Per-parameter state (heap allocated!)
    VariableV2 **params;
    TensorV2 **m;  // First moment
    TensorV2 **v;  // Second moment
    int n_params;
} AdamOptimizerV2;
```

The optimizer state is heap-allocated and might be growing.

#### 2. Tape/Graph Accumulation
The computational graph tape might be accumulating nodes despite `tape_reset()`.

#### 3. Hidden Heap Allocations
Some operations might be using `malloc()` instead of `arena_alloc()`.

---

## Workaround: Stick to Short Runs

For now, to train effectively:

### Option 1: Multiple Short Runs
```bash
# Train in 100-iteration chunks
for i in {1..50}; do
    ./train_full --small 100
    # Each run uses 1.9GB, then resets
done
```

### Option 2: Checkpointing
```bash
# Train, save checkpoint, restart
./train_full --small 100   # Iteration 0-100
# Manually load and continue from checkpoint
./train_full --small 100 --resume checkpoint_100.bin
```

---

## Investigation Plan

### Step 1: Profile Memory Allocation
Use Valgrind or similar to track all allocations:
```bash
valgrind --tool=massif ./train_full --small 1000
ms_print massif.out.<pid>
```

### Step 2: Check Optimizer State
Add logging to optimizer to see if m/v tensors are growing:
```c
printf("Optimizer memory: %ld bytes\n",
       calculate_optimizer_memory(optimizer));
```

### Step 3: Audit All malloc() Calls
Search codebase for any `malloc()` that should be `arena_alloc()`:
```bash
grep -r "malloc(" *.c | grep -v arena
```

### Step 4: Test Aggressive Cleanup Frequency
Try cleaning up every iteration instead of every 10:
```c
// In autograd_v2.c
if (iteration_count % 1 == 0) {  // Every iteration
    arena_reset_aggressive(global_arena);
}
```

---

## Temporary Recommendations

### For Demo/Testing
```bash
# Use 100 iterations (works perfectly)
./train_full --small  # Default: 100 iterations
```

### For Better Quality (with monitoring)
```bash
# Try 500 iterations and watch memory
./train_full --small 500 &
watch -n 1 'ps aux | grep train_full'
# Kill if memory exceeds 5GB
```

### For Production Training
**Don't use for long runs yet.** The system needs more debugging before it can handle 10K+ iterations.

---

## Success Criteria

We'll know the issue is fixed when:
- ✅ Can run 10,000 iterations
- ✅ Memory stays under 5GB throughout
- ✅ No manual intervention needed
- ✅ Stable performance (100 it/s)

---

## The Bigger Picture

**What's Working:**
- Infrastructure is solid (BLAS, arena allocation, BLAS wrapper)
- Short runs are production-ready
- 100 it/s performance is excellent

**What's Needed:**
- Fix the accumulation issue for long runs
- Then we can truly train good models

**Timeline:**
- Investigation: 1-2 days
- Fix: 1-2 days
- Testing: 1 day
- **Total**: 3-5 days to full fix

---

*For now, we celebrate what works (short runs, 3000x speedup, 97% memory reduction for 100 iterations) and acknowledge what needs more work (long training stability).*

*November 13, 2025*
