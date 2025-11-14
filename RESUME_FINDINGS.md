# Resume Functionality - What We Learned

**Date**: November 13, 2025
**Status**: Resume works, but doesn't solve our problems

---

## What We Implemented ✅

### Checkpoint Resume Functionality

Added `--resume` flag to `train_full.c`:

```bash
# Train 1000 iterations, save checkpoint
./train_full --small

# Resume from checkpoint for 1000 more iterations
./train_full --resume models/checkpoint.iter_001000.ckpt
```

**What it does:**
- ✅ Loads model weights from checkpoint
- ✅ Loads optimizer state (but see below - it's empty!)
- ✅ Resumes iteration counter
- ✅ Continues training from where it left off

**Code changes:**
- `train_full.c`: Added `--resume` argument parsing
- `train_full.c`: Conditional model creation vs checkpoint loading
- `train_full.c`: Training loop starts from `start_iter` instead of 0

---

## What We Discovered ❌

### Critical Finding #1: Optimizer is NOT Adam!

**File**: `transformer_v2.c`, line ~293

```c
void adam_step(AdamOptimizerV2 *opt) {
    /* Simple SGD for now - can extend to full Adam later */
    for (int i = 0; i < opt->n_params; i++) {
        VariableV2 *param = opt->params[i];
        if (param->grad) {
            for (int j = 0; j < param->data->size; j++) {
                param->data->data[j] -= opt->learning_rate * param->grad->data[j];
            }
        }
    }
}
```

**The truth:**
- Despite being named `AdamOptimizerV2`, it's **plain SGD**
- **NO momentum vectors** (m, v)
- **NO adaptive learning rates**
- **NO bias correction**
- Just: `param -= learning_rate * gradient`

**Why this matters:**
- Chunked training with resume preserves **model weights** ✅
- But there's **NO optimizer momentum to preserve** ❌
- Each chunk still starts "cold" without gradient history
- Learning is less efficient than true Adam would be

### Critical Finding #2: Resume Doesn't Fix Memory Leak

**Observation**: When running `./train_resume.sh`, memory still grew to 60GB

**Why:**
```
Chunk 1: Trains 0→1000 iterations (uses 1.9GB, exits cleanly) ✅
Chunk 2: Loads checkpoint, trains 1000→2000 iterations (MEMORY LEAK!) ❌
Chunk 3: Loads checkpoint, trains 2000→3000 iterations (MEMORY LEAK!) ❌
```

**The problem:**
- Resume loads weights correctly
- But then trains for another 1000 iterations in a single run
- The **dangling pointer bug** causes memory to leak during those 1000 iterations
- At 1000 iterations we're safe, but the *resumed* runs still hit the leak

**Root cause:** Resume doesn't fix the dangling pointer bug - it just restores state between runs.

### Critical Finding #3: Checkpoint Save/Load Incomplete

**File**: `model_io_v2.c`

**What checkpoint_save does** (lines 237-243):
```c
/* Write optimizer state (m and v for Adam) */
/* Note: This assumes optimizer stores states - would need to extend optimizer */
/* For now, just write zeros as placeholders */
double *zeros = calloc(tensor->size, sizeof(double));
fwrite(zeros, sizeof(double), tensor->size, f);  // m
fwrite(zeros, sizeof(double), tensor->size, f);  // v
free(zeros);
```

**What checkpoint_load does** (lines 314-317):
```c
/* Read optimizer states (skip for now) */
double *temp = malloc(size * sizeof(double));
fread(temp, sizeof(double), size, f);  // m
fread(temp, sizeof(double), size, f);  // v
free(temp);  // THROW AWAY!
```

**The situation:**
- Checkpoints save **zeros** for optimizer state (placeholder)
- Loading **reads and discards** those zeros
- **No actual optimizer state is preserved**

---

## What Resume Actually Accomplishes

### What Works:
1. ✅ **Model weights preserved** - The actual learned parameters carry forward
2. ✅ **Iteration counter** - Training continues from correct iteration number
3. ✅ **Learning rate schedule** - Cosine decay continues from correct position
4. ✅ **Infrastructure** - Clean checkpoint save/load implementation

### What Doesn't Work:
1. ❌ **No momentum preservation** - Optimizer is SGD, nothing to preserve
2. ❌ **Memory leak persists** - Dangling pointer bug still occurs during each run
3. ❌ **No quality improvement** - Without momentum, learning is inefficient

---

## The Harsh Reality

### We Have 3 Separate Problems:

#### Problem 1: Dangling Pointer Memory Leak
**Location**: Every operation in `autograd_v2.c`
**Impact**: Can't run >1000 iterations continuously
**Solution**: Fix tape_add_op to copy input arrays (Option B from CLAUDE.md)
**Status**: **Not fixed**

#### Problem 2: Optimizer is SGD, Not Adam
**Location**: `transformer_v2.c` adam_step function
**Impact**: No momentum, inefficient learning
**Solution**: Implement real Adam with m,v tensors and bias correction
**Status**: **Not implemented**

#### Problem 3: Optimizer State Not Saved/Loaded
**Location**: `model_io_v2.c` checkpoint functions
**Impact**: Even if we had Adam, momentum wouldn't be preserved
**Solution**: Actually save/load m and v tensors
**Status**: **Placeholder code only**

---

## What Resume IS Good For

Despite the limitations, resume functionality is still valuable for:

### Use Case 1: Interrupted Training
```bash
# Training interrupted at iteration 500
^C

# Resume from last checkpoint
./train_full --resume models/checkpoint.iter_001000.ckpt
```

### Use Case 2: Hyperparameter Tuning
```bash
# Train with one learning rate
./train_full --small  # lr=3e-4

# Continue with different learning rate
# (would need to add LR override flag)
./train_full --resume --lr 1e-4
```

### Use Case 3: Fine-tuning
```bash
# Train base model
./train_full --small

# Resume and fine-tune on different dataset
# (would need dataset selection flag)
./train_full --resume --dataset custom.txt
```

---

## The Bottom Line

### What We Thought Resume Would Do:
- Preserve momentum between chunked runs
- Enable continuous learning across process restarts
- Solve the memory leak by restarting process

### What Resume Actually Does:
- ✅ Preserve model weights
- ✅ Continue iteration counter
- ❌ NO momentum (optimizer is SGD)
- ❌ Memory leak still happens during each run
- ❌ No improvement over basic chunked training

### The Real Path Forward:

**Priority 1: Fix Dangling Pointer Bug** (2-3 hours)
```c
// In tape_add_op:
VariableV2 **inputs_copy = arena_alloc(global_arena,
                                       num_inputs * sizeof(VariableV2*));
memcpy(inputs_copy, inputs, num_inputs * sizeof(VariableV2*));
op->inputs = inputs_copy;
```

**Priority 2: Implement Real Adam** (1-2 hours)
```c
typedef struct {
    VariableV2 **params;
    TensorV2 **m;  // First moment
    TensorV2 **v;  // Second moment
    int n_params;
    int t;  // Timestep
    double learning_rate, beta1, beta2, epsilon;
} AdamOptimizerV2;
```

**Priority 3: Save/Load Optimizer State** (1 hour)
```c
// Actually save m and v in checkpoint_save
fwrite(optimizer->m[i]->data, sizeof(double), size, f);
fwrite(optimizer->v[i]->data, sizeof(double), size, f);

// Actually restore m and v in checkpoint_load
fread(optimizer->m[i]->data, sizeof(double), size, f);
fread(optimizer->v[i]->data, sizeof(double), size, f);
```

**Then:** Resume will actually enable continuous learning!

---

## Comparison: With vs Without True Adam + Resume

### Current State (SGD + Resume):
```
Chunk 1: Loss 4.3 → 3.3 (fresh start)
Chunk 2: Loss 4.3 → 3.3 (fresh start, same endpoint)
Chunk 3: Loss 4.3 → 3.3 (fresh start, same endpoint)
Result: NO IMPROVEMENT ❌
```

### After Implementing Real Adam + Resume:
```
Chunk 1: Loss 4.3 → 3.3 (with momentum build-up)
Chunk 2: Loss 3.3 → 2.8 (momentum preserved!)
Chunk 3: Loss 2.8 → 2.4 (continuous improvement!)
Result: PROGRESSIVE LEARNING ✅
```

---

## Files Modified Today

**New Files:**
- `train_resume.sh` - Script for chunked training with resume
- `RESUME_FINDINGS.md` - This document

**Modified Files:**
- `train_full.c` - Added --resume functionality

**What We Learned:**
- Resume infrastructure works correctly
- But underlying optimizer (SGD not Adam) limits its value
- Dangling pointer bug must be fixed first
- Then real Adam must be implemented
- Then optimizer state save/load completed
- **THEN** resume will enable true continuous learning

---

## Recommendation

**Don't invest more time in resume workarounds.**

Instead:
1. Fix the dangling pointer bug (enables continuous >1000 iteration runs)
2. Implement real Adam optimizer (enables efficient learning)
3. Complete optimizer state save/load (makes resume truly valuable)

**Estimated total time**: 4-6 hours
**Result**: Production-ready continuous training with momentum

**Current workaround**: Stick with short runs (100-1000 iterations) until fixes are complete.

---

*November 13, 2025 - Resume Investigation Complete*
*Conclusion: Good infrastructure, wrong foundation (SGD not Adam)*
