/*
 * FluxParser - Research-Grade C Math Parser
 * Copyright (C) 2025 Eduardo Stern
 *
 * SAMPLING UTILITIES IMPLEMENTATION
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sampling.h"

/* ============================================================================
 * TEMPERATURE SAMPLING
 * ============================================================================ */

int sample_from_logits(const double *logits, int vocab_size, double temperature) {
    /* Apply temperature scaling */
    double *scaled_logits = malloc(sizeof(double) * vocab_size);
    for (int i = 0; i < vocab_size; i++) {
        scaled_logits[i] = logits[i] / temperature;
    }

    /* Compute softmax probabilities (with numerical stability) */
    double max_logit = scaled_logits[0];
    for (int i = 1; i < vocab_size; i++) {
        if (scaled_logits[i] > max_logit) {
            max_logit = scaled_logits[i];
        }
    }

    double *probs = malloc(sizeof(double) * vocab_size);
    double sum = 0.0;
    for (int i = 0; i < vocab_size; i++) {
        probs[i] = exp(scaled_logits[i] - max_logit);
        sum += probs[i];
    }
    for (int i = 0; i < vocab_size; i++) {
        probs[i] /= sum;
    }

    /* Sample from probability distribution */
    int result = sample_from_probs(probs, vocab_size);

    free(scaled_logits);
    free(probs);

    return result;
}

int sample_from_probs(const double *probs, int vocab_size) {
    /* Multinomial sampling */
    double r = (double)rand() / RAND_MAX;
    double cumsum = 0.0;

    for (int i = 0; i < vocab_size; i++) {
        cumsum += probs[i];
        if (r < cumsum) {
            return i;
        }
    }

    /* Fallback (should rarely happen due to numerical precision) */
    return vocab_size - 1;
}

/* ============================================================================
 * TEXT GENERATION
 * ============================================================================ */

/* Note: These functions will be implemented once we have the transformer model.
 * For now, they're declared in the header but implementation is deferred.
 */

/*
char* generate_text(
    TransformerModel *model,
    Vocabulary *vocab,
    const char *prompt,
    int max_new_tokens,
    double temperature
) {
    // TODO: Implement after transformer model is ready
    return NULL;
}

void generate_and_print(
    TransformerModel *model,
    Vocabulary *vocab,
    const char *prompt,
    int max_new_tokens,
    double temperature
) {
    // TODO: Implement after transformer model is ready
}
*/
