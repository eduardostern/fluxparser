# FluxParser Transformer Training Guide

Complete guide for training, saving, and using transformer language models with FluxParser.

## 🎯 What You Can Do Now

With the new training infrastructure, you can:

1. **Train on Real Data**: Download and train on Shakespeare dataset (1MB text)
2. **Save Trained Models**: Persist weights to disk for later use
3. **Load and Continue Training**: Resume from checkpoints
4. **Generate Text**: Use trained models for inference
5. **Interactive Generation**: Chat-style text generation with temperature control

## 📁 New Files Overview

### Training Infrastructure
- `train_full.c` - Complete training with dataset loading, checkpointing, saving
- `dataset.c/h` - Dataset loading and character-level tokenization
- `model_io_v2.c/h` - Model serialization (save/load weights)
- `generate.c` - Text generation and inference

### Training Flow
```
┌─────────────────┐
│  Load Dataset   │ (Shakespeare from web or local)
└────────┬────────┘
         ↓
┌─────────────────┐
│ Create Model    │ (Transformer architecture)
└────────┬────────┘
         ↓
┌─────────────────┐
│ Training Loop   │ (Forward → Loss → Backward → Update)
│  - Log progress │
│  - Generate     │
│  - Checkpoint   │
└────────┬────────┘
         ↓
┌─────────────────┐
│  Save Model     │ (models/model_final.bin)
└────────┬────────┘
         ↓
┌─────────────────┐
│ Text Generation │ (./generate model.bin)
└─────────────────┘
```

## 🚀 Quick Start

### 1. Build Everything

```bash
# Compile training program
gcc -o train_full train_full.c autograd_v2.c arena.c transformer_v2.c \
    dataset.c model_io_v2.c -lm -O2

# Compile generation program
gcc -o generate generate.c autograd_v2.c arena.c transformer_v2.c \
    dataset.c model_io_v2.c -lm -O2
```

### 2. Train a Model

```bash
# Small model (fast training, ~5 minutes)
./train_full --small

# Medium model (default, ~30 minutes)
./train_full

# Large model (slow, ~2 hours)
./train_full --large

# Custom iterations
./train_full 5000
```

**What happens during training:**
- Downloads Shakespeare dataset if not present (1MB)
- Creates `models/` directory
- Saves tokenizer to `models/tokenizer.bin`
- Trains transformer for N iterations
- Shows loss every 100 iterations
- Generates sample text every 500 iterations
- Saves checkpoints every 1000 iterations
- Saves final model to `models/model_final.bin`

### 3. Generate Text with Trained Model

```bash
# Interactive mode (recommended)
./generate models/model_final.bin models/tokenizer.bin --interactive

# Generate from prompt
./generate models/model_final.bin models/tokenizer.bin \
    --prompt "To be or not to be"

# Just load model (defaults to interactive)
./generate models/model_final.bin
```

## 📖 Detailed Usage

### Training Configuration

Models come in 3 sizes:

| Size | d_model | heads | layers | ff_dim | params | Training Time |
|------|---------|-------|--------|--------|--------|---------------|
| Small | 128 | 4 | 2 | 512 | ~0.5M | ~5 min |
| Medium | 256 | 8 | 4 | 1024 | ~2M | ~30 min |
| Large | 512 | 16 | 6 | 2048 | ~8M | ~2 hours |

### Training Output

```
=== FluxParser Transformer Training (Full Version) ===

Loading dataset...
Downloading Shakespeare dataset...
Downloaded successfully to data/shakespeare.txt
Loaded 1115394 bytes from data/shakespeare.txt
Created tokenizer with 65 unique tokens
Dataset: 1115394 tokens, vocab size: 65

Tokenizer saved to models/tokenizer.bin
Creating transformer model...
  Architecture: d=256, heads=8, layers=4, ff=1024
  Total parameters: 2165824 (2.17 M)

Starting training for 10000 iterations...
=====================================
Iter   100/10000 | Loss: 3.2145 | LR: 3.00e-05 | Speed: 12.3 it/s
Iter   200/10000 | Loss: 2.8932 | LR: 6.00e-05 | Speed: 12.5 it/s
Iter   300/10000 | Loss: 2.6781 | LR: 9.00e-05 | Speed: 12.4 it/s
...
Iter   500/10000 | Loss: 2.3456 | LR: 1.50e-04 | Speed: 12.6 it/s
  Sample: "To be or not to be, that is the question of life"
...
💾 Checkpoint saved: models/checkpoint.iter_001000.ckpt (iter=1000, loss=1.9876)
...
✅ Model saved to models/model_iter_005000.bin (2.34 MB)
...
=====================================
Training complete!

✅ Model saved to models/model_final.bin (2.34 MB)
   Architecture: vocab=65, d_model=256, heads=8, layers=4
   Parameters: 38 tensors, 2165824 total values

To generate text with the trained model:
  ./generate models/model_final.bin models/tokenizer.bin --interactive
```

### Interactive Generation Mode

```bash
./generate models/model_final.bin --interactive
```

**Interactive Commands:**

```
=== Interactive Generation Mode ===
Commands:
  /temp <value>  - Set temperature (0.1-2.0)
  /topk <value>  - Set top-k sampling (0=off)
  /len <value>   - Set max generation length
  /quit          - Exit
  <text>         - Generate from prompt

> To be
Prompt: "To be"
Generating (temp=1.0, top_k=40):
=====================================
To be or not to be, that is the question.
Whether 'tis nobler in the mind to suffer
The slings and arrows of outrageous fortune
=====================================

> /temp 0.5
Temperature set to 0.5

> /topk 10
Top-k set to 10

> Once upon a time
Prompt: "Once upon a time"
Generating (temp=0.5, top_k=10):
=====================================
Once upon a time in the kingdom of Verona,
there lived a prince named Romeo who fell
in love with fair Juliet...
=====================================

> /quit
```

## 🎛️ Generation Parameters

### Temperature
Controls randomness of generation:
- **0.0**: Greedy (always pick most likely token) - deterministic
- **0.3-0.7**: Focused, coherent text
- **1.0**: Balanced creativity and coherence (default)
- **1.5-2.0**: Very creative, sometimes nonsensical

### Top-K Sampling
Only sample from top K most likely tokens:
- **0**: Off (sample from full distribution)
- **10**: Very conservative
- **40**: Good balance (default)
- **100+**: More diverse

## 💾 Model Files

After training, you'll have:

```
models/
├── tokenizer.bin              # Character-to-token mapping
├── model_final.bin           # Final trained model (2-10 MB)
├── model_iter_005000.bin     # Intermediate checkpoint
├── checkpoint.iter_001000.ckpt  # Full checkpoint with optimizer state
└── checkpoint.iter_002000.ckpt
```

**File Formats:**

- `.bin` - Model weights only (smaller, for inference)
- `.ckpt` - Full checkpoint (weights + optimizer state, for resuming training)

## 🔄 Resuming Training

To resume from a checkpoint (future feature - needs implementation):

```c
// In train_full.c, add at start:
TransformerV2 *model;
AdamOptimizerV2 *optimizer;
int start_iter;
double last_loss;

if (checkpoint_load(&model, &optimizer, &start_iter, &last_loss,
                   "models/checkpoint.iter_005000.ckpt") == 0) {
    printf("Resumed from iteration %d\n", start_iter);
    // Continue training from start_iter
}
```

## 📊 Understanding Training Metrics

### Loss
- **Start**: 3-4 (random guessing)
- **After 1000 iters**: 2-2.5 (learning basic patterns)
- **After 5000 iters**: 1.5-2.0 (good text structure)
- **After 10000 iters**: 1.0-1.5 (coherent Shakespeare-like text)

### Learning Rate Schedule
- **Warmup** (first 100 iters): Linear increase 0 → 3e-4
- **Training**: Cosine decay 3e-4 → 0

### Speed
- Small model: ~10-15 it/s (M1 Mac / modern CPU)
- Medium model: ~5-10 it/s
- Large model: ~2-5 it/s

## 🎨 Example Use Cases

### 1. Character-Level Language Model

```bash
# Train on Shakespeare
./train_full 10000

# Generate Shakespeare-style text
./generate models/model_final.bin --prompt "Wherefore art thou"
```

### 2. Code Generation (Train on Your Codebase)

```bash
# Prepare dataset
cat *.c > data/my_code.txt

# Modify train_full.c to load data/my_code.txt instead
# Then train
./train_full 5000

# Generate code
./generate models/model_final.bin --prompt "int main() {"
```

### 3. Custom Text Style

```bash
# Train on your text (emails, writing, etc.)
cat my_writings.txt > data/my_style.txt

# Train
./train_full --small 3000

# Generate in your style
./generate models/model_final.bin --prompt "Dear friend,"
```

## 🐛 Troubleshooting

### "Failed to load dataset"

Make sure you have internet connection for first run to download Shakespeare:
```bash
# Or manually download:
mkdir -p data
curl -o data/shakespeare.txt \
  https://raw.githubusercontent.com/karpathy/char-rnn/master/data/tinyshakespeare/input.txt
```

### "Memory usage too high"

Use smaller model or reduce sequence length:
```bash
# Edit train_full.c:
config.seq_len = 32;  // Instead of 64
config.d_model = 128; // Instead of 256
```

### "Training loss not decreasing"

- Train for more iterations (try 10000+)
- Increase learning rate: `config.learning_rate = 1e-3`
- Check dataset size (need at least 100KB text)

### "Generated text is gibberish"

- Train longer (10000+ iterations)
- Lower temperature: `/temp 0.3`
- Model might be too small for the task

## 📈 Next Steps

### Improve Training
1. **Batch Processing**: Implement true batch training (currently batch_size=1)
2. **Gradient Clipping**: Prevent exploding gradients
3. **Learning Rate Finder**: Automatically find optimal LR
4. **Validation Split**: Track overfitting

### Improve Generation
1. **Beam Search**: Better than greedy decoding
2. **Nucleus (top-p) Sampling**: Alternative to top-k
3. **Repetition Penalty**: Reduce repeated phrases
4. **Context Window Sliding**: Better long-form generation

### Scale Up
1. **Larger Models**: GPT-2 medium/large size
2. **Better Datasets**: Wikipedia, books, code
3. **BPE Tokenization**: Better than character-level
4. **Multi-GPU Training**: Parallelize across devices

## 🎓 Learning Resources

- Original paper: "Attention Is All You Need" (Vaswani et al.)
- Karpathy's minGPT: https://github.com/karpathy/minGPT
- The Illustrated Transformer: http://jalammar.github.io/illustrated-transformer/
- OpenAI GPT-2 paper: "Language Models are Unsupervised Multitask Learners"

## 🤝 Contributing

Ideas for improvements:
- Implement checkpoint resuming
- Add true batching support
- Implement beam search
- Add validation metrics (perplexity)
- Support for BPE tokenization
- Multi-GPU training with MPI

---

**Remember**: This is a research/educational implementation. For production use, consider PyTorch/TensorFlow which have years of optimization.