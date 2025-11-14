#!/bin/bash
# Chunked training script: 10,000 iterations on Shakespeare dataset
# Avoids memory leak by restarting process between chunks

TOTAL_CHUNKS=10
ITERS_PER_CHUNK=1000
TOTAL_ITERS=$((TOTAL_CHUNKS * ITERS_PER_CHUNK))

echo "========================================="
echo "CHUNKED TRAINING: Shakespeare Dataset"
echo "========================================="
echo "Total iterations: $TOTAL_ITERS"
echo "Chunks: $TOTAL_CHUNKS × $ITERS_PER_CHUNK iterations"
echo "Model: --small (d=128, heads=4, layers=2)"
echo "========================================="
echo ""

START_TIME=$(date +%s)

for i in $(seq 1 $TOTAL_CHUNKS); do
    echo "🔄 Chunk $i/$TOTAL_CHUNKS (iterations $(( (i-1) * ITERS_PER_CHUNK + 1 )) - $(( i * ITERS_PER_CHUNK )))"
    echo "   Started: $(date '+%H:%M:%S')"

    # Run training chunk
    ./train_full --small 2>&1 | grep -E "(Loss:|Sample:|saved|Parameters|Dataset)"

    EXIT_CODE=$?
    if [ $EXIT_CODE -ne 0 ]; then
        echo "❌ Training failed in chunk $i (exit code: $EXIT_CODE)"
        exit 1
    fi

    echo "   Completed: $(date '+%H:%M:%S')"
    echo ""

    # Brief pause between chunks
    sleep 1
done

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))
MINUTES=$((ELAPSED / 60))
SECONDS=$((ELAPSED % 60))

echo "========================================="
echo "✅ TRAINING COMPLETE!"
echo "========================================="
echo "Total iterations: $TOTAL_ITERS"
echo "Total time: ${MINUTES}m ${SECONDS}s"
echo "Average: $(echo "scale=1; $ELAPSED / $TOTAL_CHUNKS" | bc)s per chunk"
echo ""
echo "Model saved to: models/model_final.bin"
echo "Tokenizer: models/tokenizer.bin"
echo ""
echo "Test generation:"
echo "  ./generate_v2 models/model_final.bin --prompt \"To be or not to be\""
echo "========================================="
