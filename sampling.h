/*
 * FluxParser - Research-Grade C Math Parser
 * Copyright (C) 2025 Eduardo Stern
 *
 * SAMPLING UTILITIES - Text Generation
 * Temperature scaling, multinomial sampling, and generation loops
 */

#ifndef SAMPLING_H
#define SAMPLING_H

#include "autograd_v2.h"
#include "text_utils.h"

/* ============================================================================
 * TEMPERATURE SAMPLING
 * ============================================================================ */

/* Sample index from logits with temperature
 * temperature = 1.0: use raw probabilities
 * temperature < 1.0: more confident (sharper distribution)
 * temperature > 1.0: more random (flatter distribution)
 */
int sample_from_logits(const double *logits, int vocab_size, double temperature);

/* Sample index from probability distribution */
int sample_from_probs(const double *probs, int vocab_size);

/* ============================================================================
 * TEXT GENERATION
 * ============================================================================ */

typedef struct TransformerModel TransformerModel;  /* Forward declaration */

/* Generate text from prompt
 * Returns newly allocated string (caller must free)
 */
char* generate_text(
    TransformerModel *model,
    Vocabulary *vocab,
    const char *prompt,
    int max_new_tokens,
    double temperature
);

/* Generate and print text with streaming output */
void generate_and_print(
    TransformerModel *model,
    Vocabulary *vocab,
    const char *prompt,
    int max_new_tokens,
    double temperature
);

#endif /* SAMPLING_H */
