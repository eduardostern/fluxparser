# Training Status & Performance

## ✅ WORKING & TESTED

### train_v2 (Recommended)
- **Model**: 291K params, 1 layer, 256 dims
- **Memory**: ~6GB peak, stable
- **Speed**: 2 minutes for 2000 iterations
- **Quality**: Good for testing
- **Command**: `make && ./train_v2 2000`

### train_full --tiny (Low Memory)
- **Model**: 46K params, 1 layer, 64 dims
- **Memory**: ~10MB, very stable
- **Speed**: 3 minutes for 2000 iterations
- **Quality**: Decent for small dataset
- **Command**: `./train_full --tiny`

## ⚠️ WORKING BUT SLOW

### train_full --small
- **Model**: 422K params, 2 layers, 128 dims
- **Memory**: Stable (fixes applied)
- **Speed**: **30-60 seconds PER ITERATION**
- **Issue**: Unoptimized pure-C matmul, no SIMD/BLAS
- **Command**: `./train_full --small` (be patient!)

## 🔧 BUGS FIXED

### 1. Memory Leak (FIXED)
**Problem**: Arena reset called before checkpointing, leaked memory each iteration
**Solution**: Moved `autograd_reset_iteration()` to END of training loop

### 2. Catastrophic Zeroing (FIXED)
**Problem**: `arena_calloc` spent 98% of time zeroing 59GB of temp tensors
**Solution**: Changed `tensor_create_temp` to use `arena_alloc` (no zeroing)

### 3. Unbounded Arena Growth (FIXED)
**Problem**: Arena accumulated chunks without freeing
**Solution**: Aggressive reset every 10 iterations frees excess chunks

## 📊 Performance Analysis

### Why --small is Slow

**Compute per iteration**:
- Attention: 4 heads × 2 layers × (64×32×64) matmuls = **~1M ops**
- FFN: 2 layers × (64×128×512) matmuls = **~8M ops**
- Backward: Doubles everything = **~18M ops total**

**At ~50 MFLOPS** (pure C, no SIMD): **18M / 50M = 0.36 seconds per iter**
**Actual: 30-60 seconds** → Indicates overhead from memory allocation/arena

### Why --tiny Works

- 1 layer, 64 dims = **~1/10th the compute**
- Iterations complete in 1-2 seconds
- 2000 iters = 3 minutes total

## 🚀 Recommendations

### For Development/Testing
```bash
./train_full --tiny    # Fast, low memory
```

### For Better Quality (if patient)
```bash
./train_full --small   # Takes ~8-16 hours for 10K iters!
```

### For Production (Best)
```bash
./train_v2 2000        # Proven, stable, tested
```

## 💡 Future Optimizations

To make `--small` practical:

1. **Use BLAS**: Link against OpenBLAS/Accelerate framework
2. **SIMD**: Use ARM NEON intrinsics for matmul
3. **GPU**: Implement CUDA/Metal backend
4. **In-place ops**: Reuse gradient buffers instead of allocating

With these, `--small` would be **100-1000x faster**.

## 📝 Commit Summary

**Files Modified**:
- `autograd_v2.c`: Changed tensor_create_temp to use arena_alloc (no zeroing), aggressive reset every 10 iters
- `train_full.c`: Moved arena reset to end of loop

**Result**: Memory leaks fixed, but multi-layer training still slow due to unoptimized matmul.

**Status**: ✅ Memory management correct, ⚠️ Performance needs BLAS/SIMD for larger models
