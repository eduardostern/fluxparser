CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -pthread
LDFLAGS = -lm -pthread

# Detect platform and add BLAS if available
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS: Use built-in Accelerate framework
    LDFLAGS += -framework Accelerate
    $(info 🚀 Using Apple Accelerate framework for BLAS acceleration)
else ifeq ($(USE_OPENBLAS),1)
    # Linux with OpenBLAS
    CFLAGS += -DUSE_OPENBLAS
    LDFLAGS += -lopenblas
    $(info 🚀 Using OpenBLAS for acceleration)
else
    $(info ⚠️  No BLAS found - using pure C (slower))
endif

# Autograd V2 - New memory-safe transformer training system
V2_TARGETS = train_v2 train_full generate_v2 test_layer_norm test_attention test_transformer_backward
V2_OBJS = autograd_v2.o arena.o transformer_v2.o blas_wrapper.o
V2_HEADERS = autograd_v2.h arena.h transformer_v2.h model_io_v2.h dataset.h blas_wrapper.h

# Legacy targets
TARGETS = parser_test test_vars example_usage test_safety demo_safety test_advanced test_research test_calculus test_numerical test_new_features calculate_pi test_advanced_features test_optimizer demo_curve_fit test_tensor demo_xor_nn test_autograd demo_xor_autograd demo_debug_tools demo_transformer_lm demo_prompt demo_tiny_lm demo_working demo_readable train_big train_medium generate
HEADERS = parser.h ast.h autograd.h text_utils.h sampling.h transformer.h model_io.h
TENSOR_OBJS = tensor.o
AUTOGRAD_OBJS = tensor.o autograd.o
TEXT_OBJS = text_utils.o sampling.o
TRANSFORMER_OBJS = tensor.o autograd.o text_utils.o sampling.o transformer.o

.PHONY: all clean run v2 help

# Default: build V2 system
all: $(V2_TARGETS)

# Build everything (V2 + legacy)
full: $(V2_TARGETS) $(TARGETS)

# Build only V2 system
v2: $(V2_TARGETS)

# Build legacy system
legacy: $(TARGETS)

# ============================================================================
# AUTOGRAD V2 TARGETS (Memory-Safe Transformer Training)
# ============================================================================

# Core V2 library
autograd_v2.o: autograd_v2.c autograd_v2.h arena.h blas_wrapper.h
	$(CC) $(CFLAGS) -c autograd_v2.c

arena.o: arena.c arena.h
	$(CC) $(CFLAGS) -c arena.c

blas_wrapper.o: blas_wrapper.c blas_wrapper.h
	$(CC) $(CFLAGS) -c blas_wrapper.c

transformer_v2.o: transformer_v2.c transformer_v2.h autograd_v2.h
	$(CC) $(CFLAGS) -c transformer_v2.c

dataset.o: dataset.c dataset.h
	$(CC) $(CFLAGS) -c dataset.c

model_io_v2.o: model_io_v2.c model_io_v2.h transformer_v2.h
	$(CC) $(CFLAGS) -c model_io_v2.c

# Training programs
train_v2: train_v2.c $(V2_OBJS) sampling.o text_utils.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

train_full: train_full.c $(V2_OBJS) dataset.o model_io_v2.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Text generation
generate_v2: generate.c $(V2_OBJS) dataset.o model_io_v2.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Tests
test_layer_norm: test_layer_norm.c $(V2_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_attention: test_attention.c $(V2_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_transformer_backward: test_transformer_backward.c $(V2_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ============================================================================
# LEGACY TARGETS
# ============================================================================

parser_test: test.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_vars: test_vars.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

example_usage: example_usage.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_safety: test_safety.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_safety: demo_safety.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_advanced: test_advanced.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_research: test_research.o ast.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_calculus: test_calculus.o ast.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_numerical: test_numerical.o ast.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_new_features: test_new_features.o ast.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

calculate_pi: calculate_pi.o ast.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_advanced_features: test_advanced_features.o ast.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_optimizer: test_optimizer.o ast.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_curve_fit: demo_curve_fit.o ast.o parser.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_tensor: test_tensor.o ast.o parser.o tensor.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_xor_nn: demo_xor_nn.o ast.o parser.o tensor.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_autograd: test_autograd.o ast.o parser.o tensor.o autograd.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_xor_autograd: demo_xor_autograd.o ast.o parser.o tensor.o autograd.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_debug_tools: demo_debug_tools.o ast.o parser.o tensor.o autograd.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_transformer_lm: demo_transformer_lm.o ast.o parser.o tensor.o autograd.o text_utils.o sampling.o transformer.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_prompt: demo_prompt.o ast.o parser.o tensor.o autograd.o text_utils.o sampling.o transformer.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_tiny_lm: demo_tiny_lm.o ast.o parser.o tensor.o autograd.o text_utils.o sampling.o transformer.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_working: demo_working.o ast.o parser.o tensor.o autograd.o text_utils.o sampling.o transformer.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

demo_readable: demo_readable.o ast.o parser.o tensor.o autograd.o text_utils.o sampling.o transformer.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

train_big: train_big.o ast.o parser.o tensor.o autograd.o text_utils.o sampling.o transformer.o model_io.o arena.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

train_medium: train_medium.o ast.o parser.o tensor.o autograd.o text_utils.o sampling.o transformer.o model_io.o arena.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

generate: generate.o ast.o parser.o tensor.o autograd.o text_utils.o sampling.o transformer.o model_io.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(TARGETS) $(V2_TARGETS)
	rm -rf models/*.ckpt

help:
	@echo "FluxParser Build System"
	@echo "======================="
	@echo ""
	@echo "Autograd V2 (Recommended):"
	@echo "  make              - Build V2 training system (train_full, generate_v2)"
	@echo "  make v2           - Same as 'make'"
	@echo "  make train_full   - Build full training with dataset loading"
	@echo "  make generate_v2  - Build text generation tool"
	@echo ""
	@echo "Quick Start (Low Memory):"
	@echo "  make && ./train_full --tiny     # ~10MB, 2min"
	@echo "  ./generate_v2 models/model_final.bin --interactive"
	@echo ""
	@echo "Full Training:"
	@echo "  ./train_full --small            # ~50MB, 5min"
	@echo "  ./train_full --medium           # ~500MB, 30min (downloads 1MB Shakespeare)"
	@echo ""
	@echo "Other targets:"
	@echo "  make full         - Build V2 + legacy systems"
	@echo "  make legacy       - Build legacy v1 system"
	@echo "  make clean        - Remove all build artifacts"
	@echo "  make help         - Show this help"
	@echo ""
	@echo "Documentation:"
	@echo "  TRAINING_GUIDE.md          - Complete training guide"
	@echo "  AUTOGRAD_V2_ARCHITECTURE.md - Technical architecture"
	@echo "  AUTOGRAD_V2_QUICKSTART.md   - API quick reference"

run: parser_test
	./parser_test
