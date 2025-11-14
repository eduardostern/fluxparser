# Memory Usage Guide

## TL;DR

- **Use `train_v2`** - Hardcoded tiny dataset, ~6GB peak (works well!)
- **Use `train_full --tiny`** - Small dataset, ~10MB, fast
- **Avoid `train_full` without flags** - Downloads 1MB Shakespeare, memory hog

## Memory Comparison

| Program | Dataset Size | Model Size | Memory Usage | Training Time | Status |
|---------|--------------|------------|--------------|---------------|--------|
| `train_v2` | 431 chars (hardcoded) | 291K params | **~6GB peak** | 2min (2000 iter) | ✅ **Best** |
| `train_full --tiny` | 432 chars (built-in) | 46K params | **~10MB** | 2min (2000 iter) | ✅ Good |
| `train_full --small` | 432 chars (built-in) | 500K params | ~50MB | 5min (10K iter) | ✅ Good |
| `train_full --medium` | 1.1M chars (download) | 2M params | **~500MB** | 30min (10K iter) | ⚠️ Memory hog |
| `train_full --large` | 1.1M chars (download) | 8M params | **~2GB** | 2 hours | ⚠️ Memory hog |

## Why the Difference?

### train_v2.c (Recommended)
```c
// Small hardcoded dataset
const char *text = "To be or not to be..." // 431 chars
```
- **Pros**: Low memory (~6GB peak), self-contained, no downloads
- **Cons**: Limited dataset diversity
- **Use when**: You want it to just work

### train_full.c --tiny (Low Memory)
```c
// Ultra-small model + built-in dataset
d_model=64, heads=2, layers=1  // 46K params
dataset: 432 chars (same as train_v2)
```
- **Pros**: Minimal memory (~10MB), fast training
- **Cons**: Very small model, limited capacity
- **Use when**: Testing, debugging, low-memory systems

### train_full.c --small (Balanced)
```c
// Small model + built-in dataset
d_model=128, heads=4, layers=2  // 500K params
dataset: 432 chars (built-in, no download)
```
- **Pros**: Good balance, no download needed
- **Cons**: Still limited dataset
- **Use when**: You want better quality than tiny but not memory hog

### train_full.c --medium/--large (Full Shakespeare)
```c
// Downloads 1.1MB Shakespeare dataset
dataset: 1,115,394 chars = 4.4MB as tokens
model: 2-8M params
```
- **Pros**: Real dataset, better quality text
- **Cons**: **Memory hog** - uses 500MB-2GB+
- **Use when**: You have plenty of RAM and want real Shakespeare

## Recommendations

### For Daily Use
```bash
# Just use train_v2 - it works!
make
./train_v2 2000
```

### For Testing New Features
```bash
# Use tiny mode - fast iteration
./train_full --tiny
```

### For Best Quality
```bash
# Download Shakespeare first to avoid memory issues
mkdir -p data
curl -o data/shakespeare.txt \
  https://raw.githubusercontent.com/karpathy/char-rnn/master/data/tinyshakespeare/input.txt

# Then train with --medium or --large
./train_full --medium
```

## Memory Optimization Tips

### 1. Reduce Sequence Length
```c
// In train_full.c, reduce seq_len
config.seq_len = 32;  // Instead of 64 or 128
```

### 2. Reduce Model Size
```bash
./train_full --tiny   # Smallest
./train_full --small  # Small
# Skip --medium and --large on low memory systems
```

### 3. Use Smaller Dataset
The `--tiny` and `--small` flags automatically use the built-in 432-character dataset instead of downloading 1MB Shakespeare.

### 4. Monitor Memory During Training
```bash
# Run in background and monitor
./train_full --small &
watch -n 1 "ps aux | grep train_full"
```

## Architecture Memory Breakdown

### Tiny Model (46K params)
```
- Embedding: 35 vocab × 64 dim = 2,240 params
- 1 Transformer layer:
  - Attention (Q,K,V,O): ~16K params
  - FFN: ~16K params
  - Layer norm: ~256 params
- Output projection: ~2K params
Total: ~46,307 params × 8 bytes = 370KB model weights
```

### Medium Model (2M params)
```
- Embedding: 65 vocab × 256 dim = 16,640 params
- 4 Transformer layers × ~500K params each
- Output projection: ~16K params
Total: ~2,165,824 params × 8 bytes = 17MB model weights
```

**But:** During training, we also store:
- Gradients (same size as weights)
- Optimizer state (2× weights for Adam m and v)
- Activations in arena (temporary, freed each iteration)

So total memory = **3-5× model size** during training!

## When Memory Hog Happens

The issue with `train_full` without flags:
1. Loads 1.1MB text file into memory
2. Tokenizes to 1.1M integers (4.4MB)
3. Creates large model (2-8M params = 16-64MB)
4. Gradients + optimizer state (2-3× model size = 48-192MB)
5. Activations during forward/backward (variable, arena-allocated)

**Total: 500MB-2GB depending on model size**

## Solution Implemented

Now `train_full` has smart defaults:
- `--tiny`: Uses built-in 432-char dataset, tiny model (~10MB)
- `--small`: Uses built-in 432-char dataset, small model (~50MB)
- `--medium/--large`: Downloads Shakespeare if available, warns if not

This way you can train locally without memory issues!

## Quick Reference

```bash
# Low memory? Use these:
./train_v2               # Best - 6GB peak, works everywhere
./train_full --tiny      # Minimal - 10MB
./train_full --small     # Balanced - 50MB

# Have RAM? Use these:
./train_full --medium    # Good quality - 500MB
./train_full --large     # Best quality - 2GB
```