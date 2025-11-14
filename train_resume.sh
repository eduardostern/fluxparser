#!/bin/bash
# Chunked training with checkpoint resume
# Preserves model weights between chunks for continuous learning!

CHUNKS=3
ITERS_PER_CHUNK=1000
CHECKPOINT_PATH="models/checkpoint.iter_001000.ckpt"

echo "========================================="
echo "CHUNKED TRAINING WITH RESUME"
echo "========================================="
echo "Total: $CHUNKS chunks × $ITERS_PER_CHUNK iterations"
echo "Model weights preserved between chunks!"
echo "========================================="
echo ""

START_TIME=$(date +%s)

# First chunk: train from scratch
echo "🔄 Chunk 1/$CHUNKS (iterations 0-1000) - Fresh start"
./train_full --small 2>&1 | grep -E "(Loss:|Sample:|Dataset:|Parameters)"
echo ""

# Remaining chunks: resume from checkpoint
for i in $(seq 2 $CHUNKS); do
    PREV_ITER=$(( (i-1) * ITERS_PER_CHUNK ))
    CURR_ITER=$(( i * ITERS_PER_CHUNK ))

    echo "🔄 Chunk $i/$CHUNKS (iterations $PREV_ITER-$CURR_ITER) - Resuming from checkpoint"

    ./train_full --resume $CHECKPOINT_PATH 2>&1 | grep -E "(Loss:|Sample:|Resumed|Parameters)"

    EXIT_CODE=$?
    if [ $EXIT_CODE -ne 0 ]; then
        echo "❌ Training failed in chunk $i"
        exit 1
    fi

    echo ""
    sleep 1
done

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))
MINUTES=$((ELAPSED / 60))
SECONDS=$((ELAPSED % 60))

echo "========================================="
echo "✅ TRAINING COMPLETE WITH CONTINUITY!"
echo "========================================="
echo "Total iterations: $(( CHUNKS * ITERS_PER_CHUNK ))"
echo "Total time: ${MINUTES}m ${SECONDS}s"
echo ""
echo "🎯 Model weights preserved across chunks"
echo "🎯 Loss should improve progressively!"
echo ""
echo "Model: models/model_final.bin"
echo "Test: ./generate_v2 models/model_final.bin --prompt \"To be\""
echo "========================================="
